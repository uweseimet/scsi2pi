//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_executor.h"
#include <sstream>
#include "base/device_factory.h"
#include "base/property_handler.h"
#include "command_context.h"
#include "command_image_support.h"
#include "controllers/abstract_controller.h"
#include "controllers/controller_factory.h"
#include "devices/disk.h"
#include "devices/scsi_generic.h"
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_exceptions.h"

using namespace s2p_interface_util;
using namespace s2p_util;

bool CommandExecutor::ProcessDeviceCmd(const CommandContext &context, const PbDeviceDefinition &pb_device, bool dryRun)
{
    const string &msg = PrintCommand(context.GetCommand(), pb_device);
    if (dryRun) {
        s2p_logger.trace("Validating: " + msg);
    }
    else {
        s2p_logger.info("Executing: " + msg);
    }

    if (!ValidateDevice(context, pb_device)) {
        return false;
    }

    const PbOperation operation = context.GetCommand().operation();

    // ATTACH does not require an existing device
    if (operation == ATTACH) {
        return Attach(context, pb_device, dryRun);
    }

    const auto device = controller_factory.GetDeviceForIdAndLun(pb_device.id(), pb_device.unit());
    if (!ValidateOperation(context, *device)) {
        return false;
    }

    switch (operation) {
    case DETACH:
        return Detach(context, *device, dryRun);

    case START:
        return dryRun ? true : Start(*device);

    case STOP:
        return dryRun ? true : Stop(*device);

    case INSERT:
        return Insert(context, pb_device, device, dryRun);

    case EJECT:
        return dryRun ? true : Eject(*device);

    case PROTECT:
        return dryRun ? true : Protect(*device);

    case UNPROTECT:
        return dryRun ? true : Unprotect(*device);

    default:
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION, to_string(static_cast<int>(operation)));
    }

    return false;
}

bool CommandExecutor::ProcessCmd(const CommandContext &context)
{
    const PbCommand &command = context.GetCommand();
    const PbOperation &operation = command.operation();

    // Handle commands that are not device-specific
    switch (operation) {
    case DETACH_ALL:
        DetachAll();
        return context.ReturnSuccessStatus();

    case RESERVE_IDS:
        if (const string &error = SetReservedIds(GetParam(command, "ids")); !error.empty()) {
            return context.ReturnErrorStatus(error);
        }
        else {
            PropertyHandler::GetInstance().AddProperty("reserved_ids", Join(reserved_ids, ","));
            return context.ReturnSuccessStatus();
        }

    case CHECK_AUTHENTICATION:
    case NO_OPERATION:
        // Do nothing, just log
        s2p_logger.trace("Received {} command", PbOperation_Name(operation));
        return context.ReturnSuccessStatus();

    default:
        // This is a device-specific command handled below
        break;
    }

    // Remember the list of reserved files during the dry run
    const auto &reserved_files = StorageDevice::GetReservedFiles();
    const bool isDryRunError = ranges::find_if_not(command.devices(), [this, &context](const auto &device)
        {   return ProcessDeviceCmd(context, device, true);}) != command.devices().end();
    StorageDevice::SetReservedFiles(reserved_files);

    if (isDryRunError) {
        return false;
    }

    if (!EnsureLun0(context, command)) {
        return false;
    }

    if (ranges::find_if_not(command.devices(), [this, &context](const auto &device)
        {   return ProcessDeviceCmd(context, device, false);}) != command.devices().end()) {
        return false;
    }

    // ATTACH, DETACH, INSERT and EJECT are special cases because they return the current device list
    return
        operation == ATTACH || operation == DETACH || operation == INSERT || operation == EJECT ?
            true : context.ReturnSuccessStatus();
}

bool CommandExecutor::Start(PrimaryDevice &device) const
{
    s2p_logger.info("Start requested for {}", GetIdentifier(device));

    if (!device.Start()) {
        s2p_logger.warn("Starting {} failed", GetIdentifier(device));
    }

    return true;
}

bool CommandExecutor::Stop(PrimaryDevice &device) const
{
    s2p_logger.info("Stop requested for {}", GetIdentifier(device));

    device.Stop();

    device.SetStatus(SenseKey::NO_SENSE, Asc::NO_ADDITIONAL_SENSE_INFORMATION);

    return true;
}

bool CommandExecutor::Eject(PrimaryDevice &device) const
{
    s2p_logger.info("Eject requested for {}", GetIdentifier(device));

    if (device.Eject(true)) {
        // Remove both potential properties, with and without LUN
        PropertyHandler::GetInstance().RemoveProperties(
            fmt::format("{0}{1}:{2}.params", PropertyHandler::DEVICE, device.GetId(), device.GetLun()));
        PropertyHandler::GetInstance().RemoveProperties(
            fmt::format("{0}{1}.params", PropertyHandler::DEVICE, device.GetId()));
    }
    else {
        s2p_logger.warn("Ejecting {} failed", GetIdentifier(device));
    }

    return true;
}

bool CommandExecutor::Protect(PrimaryDevice &device) const
{
    s2p_logger.info("Write protection requested for {}", GetIdentifier(device));

    device.SetProtected(true);

    return true;
}

bool CommandExecutor::Unprotect(PrimaryDevice &device) const
{
    s2p_logger.info("Write unprotection requested for {}", GetIdentifier(device));

    device.SetProtected(false);

    return true;
}

bool CommandExecutor::Attach(const CommandContext &context, const PbDeviceDefinition &pb_device, bool dryRun)
{
    const PbDeviceType type = pb_device.type();
    const int lun = pb_device.unit();

    if (const int lun_max = GetLunMax(type); lun >= lun_max) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(lun_max - 1));
    }

    const int id = pb_device.id();
    if (controller_factory.GetDeviceForIdAndLun(id, lun)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DUPLICATE_ID, to_string(id), to_string(lun));
    }

    if (reserved_ids.contains(id)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_RESERVED_ID, to_string(id));
    }

    const auto device = CreateDevice(context, pb_device);
    if (!device) {
        return false;
    }

    param_map params = { pb_device.params().cbegin(), pb_device.params().cend() };
    if (!device->SupportsImageFile()) {
        // Legacy clients like PiSCSI's scsictl might have sent both "file" and "interfaces"
        params.erase("file");
    }
    device->SetParams(params);

    const PbCachingMode caching_mode =
        pb_device.caching_mode() == PbCachingMode::DEFAULT ? PbCachingMode::PISCSI : pb_device.caching_mode();
    if (caching_mode == PbCachingMode::DEFAULT) {
        // The requested caching mode is not available for this device type
        return false;
    }

    if (!SetScsiLevel(context, *device, pb_device.scsi_level())) {
        return false;
    }

    if (!SetProductData(context, pb_device, *device)) {
        return false;
    }

    if (!SetBlockSize(context, device, pb_device.block_size())) {
        return false;
    }

#ifdef BUILD_STORAGE_DEVICE
    if (device->SupportsImageFile()) {
        const string &filename = GetParam(pb_device, "file");

        // If no filename was provided the medium is considered not inserted
        device->SetRemoved(filename.empty());

        // The caching mode must be set before the file is accessed
        if (const auto disk = dynamic_pointer_cast<Disk>(device); disk) {
            disk->SetCachingMode(caching_mode);
        }

        // Only with removable media drives, CD and MO the medium (=file) may be inserted later
        if (!device->IsRemovable() && filename.empty()) {
            // GetIdentifier() cannot be used here because the device ID has not yet been set
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME,
                fmt::format("{0} {1}:{2}", GetTypeString(*device), id, lun));
        }

        if (!ValidateImageFile(context, *static_pointer_cast<StorageDevice>(device), filename)) {
            return false;
        }
    }
#endif

    // Only non read-only devices support protect/unprotect
    // This operation must not be executed before Open() because Open() overrides some settings.
    if (device->IsProtectable() && !device->IsReadOnly()) {
        device->SetProtected(pb_device.protected_());
    }

    // Stop the dry run here, before attaching
    if (dryRun) {
        return true;
    }

    if (const string &error = device->Init(); !error.empty()) {
        s2p_logger.error(error);
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INITIALIZATION,
            fmt::format("{0} {1}:{2}", GetTypeString(*device), id, lun));
    }

    // Set the final data (they may have been overriden during the initialization of SCSG)
    SetScsiLevel(context, *device, pb_device.scsi_level());
    SetProductData(context, pb_device, *device);

    if (!controller_factory.AttachToController(bus, id, device)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_CONTROLLER);
    }

#ifdef BUILD_STORAGE_DEVICE
    if (!device->IsRemoved() && device->SupportsImageFile()) {
        static_pointer_cast<StorageDevice>(device)->ReserveFile();
    }
#endif

    SetUpDeviceProperties(device);

    DisplayDeviceInfo(*device);

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::Insert(const CommandContext &context, const PbDeviceDefinition &pb_device,
    const shared_ptr<PrimaryDevice> device, bool dryRun) const
{
    if (!device->SupportsImageFile()) {
        return false;
    }

#ifdef BUILD_STORAGE_DEVICE
    if (!device->IsRemoved()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_EJECT_REQUIRED);
    }

    if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_NAME_UPDATE);
    }

    // It has been ensured above that this cast cannot fail
    auto storage_device = static_pointer_cast<StorageDevice>(device);

    string filename = GetParam(pb_device, "file");
    if (filename.empty()) {
        filename = storage_device->GetLastFilename();
    }
    if (filename.empty()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME,
            GetIdentifier(*storage_device));
    }

    // Stop the dry run here, before modifying the device
    if (dryRun) {
        return true;
    }

    s2p_logger.info(
        "Insert " + string(pb_device.protected_() ? "protected " : "") + "file '" + filename + "' requested into "
        + GetIdentifier(*storage_device));

    if (!SetBlockSize(context, storage_device, pb_device.block_size())) {
        return false;
    }

    if (!ValidateImageFile(context, *storage_device, filename)) {
        return false;
    }

    if (!storage_device->ReserveFile()) {
        return false;
    }

    storage_device->SetMediumChanged(true);
    storage_device->SetProtected(pb_device.protected_());

    SetUpDeviceProperties(storage_device);
#endif

    return true;
}
#pragma GCC diagnostic pop

bool CommandExecutor::Detach(const CommandContext &context, PrimaryDevice &device, bool dryRun) const
{
    auto *controller = device.GetController();
    assert(controller);

    // LUN 0 can only be detached if there is no other LUN anymore
    if (!device.GetLun() && controller->GetLunCount() > 1) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_LUN0);
    }

    if (!dryRun) {
        // Remember device data before they become invalid on removal
        const int id = device.GetId();
        const int lun = device.GetLun();
        const string &identifier = GetIdentifier(device) + ", " + device.GetIdentifier();

        if (!controller->RemoveDevice(device)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
        }

        // Remove both potential identifiers
        PropertyHandler::GetInstance().RemoveProperties(fmt::format("{0}{1}:{2}.", PropertyHandler::DEVICE, id, lun));
        PropertyHandler::GetInstance().RemoveProperties(fmt::format("{0}{1}.", PropertyHandler::DEVICE, id));

        // If no LUN is left also delete the controller
        if (!controller->GetLunCount() && !controller_factory.DeleteController(*controller)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
        }

        s2p_logger.info("Detached " + identifier);
    }

    return true;
}

void CommandExecutor::DetachAll() const
{
    if (controller_factory.DeleteAllControllers()) {
        PropertyHandler::GetInstance().RemoveProperties(PropertyHandler::DEVICE);

        s2p_logger.info("Detached all devices");
    }
}

void CommandExecutor::SetUpDeviceProperties(shared_ptr<PrimaryDevice> device)
{
    const string &identifier = fmt::format("{0}{1}:{2}.", PropertyHandler::DEVICE, device->GetId(), device->GetLun());
    PropertyHandler::GetInstance().AddProperty(identifier + "type", GetTypeString(*device));
    const auto& [vendor, product, revision] = device->GetProductData();
    PropertyHandler::GetInstance().AddProperty(identifier + "name", vendor + ":" + product + ":" + revision);
#ifdef BUILD_STORAGE_DEVICE
    if (device->SupportsImageFile()) {
        const auto storage_device = static_pointer_cast<StorageDevice>(device);
        if (storage_device->GetConfiguredBlockSize()) {
            PropertyHandler::GetInstance().AddProperty(identifier + "block_size",
                to_string(storage_device->GetConfiguredBlockSize()));
        }
        string filename = storage_device->GetFilename();
        if (!filename.empty()) {
            if (filename.starts_with(CommandImageSupport::GetInstance().GetDefaultFolder())) {
                filename = filename.substr(CommandImageSupport::GetInstance().GetDefaultFolder().length() + 1);
            }
            PropertyHandler::GetInstance().AddProperty(identifier + "params", filename);
            return;
        }
    }
#endif

    if (!device->GetParams().empty()) {
        vector<string> p;
        for (const auto& [param, value] : device->GetParams()) {
            p.emplace_back(param + "=" + value);
        }
        PropertyHandler::GetInstance().AddProperty(identifier + "params", Join(p, ":"));
    }
}

void CommandExecutor::DisplayDeviceInfo(const PrimaryDevice &device) const
{
    string msg = "Attached ";
    if (device.IsReadOnly()) {
        msg += "read-only ";
    }
    else if (device.IsProtectable() && device.IsProtected()) {
        msg += "protected ";
    }
    msg += GetIdentifier(device) + ", " + device.GetIdentifier();

    s2p_logger.info(msg);
}

string CommandExecutor::SetReservedIds(const string &ids)
{
    set<int> ids_to_reserve;
    stringstream ss(ids);
    string id;
    while (getline(ss, id, ',')) {
        const int res_id = ParseAsUnsignedInt(id);
        if (res_id == -1 || res_id > 7) {
            return "Invalid ID " + id;
        }

        if (controller_factory.HasController(res_id)) {
            return "ID " + id + " is currently in use";
        }

        ids_to_reserve.insert(res_id);
    }

    reserved_ids = { ids_to_reserve.cbegin(), ids_to_reserve.cend() };

    if (ids_to_reserve.empty()) {
        s2p_logger.info("Cleared reserved ID(s)");
    }
    else {
        s2p_logger.info("Reserved ID(s) set to {}", Join(ids_to_reserve));
    }

    return "";
}

#ifdef BUILD_STORAGE_DEVICE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::ValidateImageFile(const CommandContext &context, StorageDevice &device,
    const string &filename) const
{
    if (filename.empty()) {
        return true;
    }

    if (!CheckForReservedFile(context, filename)) {
        return false;
    }

    string effective_filename = filename;

    if (!StorageDevice::FileExists(filename)) {
        // If the file does not exist search for it in the default image folder
        effective_filename = CommandImageSupport::GetInstance().GetDefaultFolder() + "/" + filename;

        if (!CheckForReservedFile(context, effective_filename)) {
            return false;
        }
    }

    device.SetFilename(effective_filename);

    try {
        device.Open();
    }
    catch (const IoException &e) {
        s2p_logger.error(e.what());

        return context.ReturnLocalizedError(LocalizationKey::ERROR_FILE_OPEN, device.GetFilename());
    }

    return true;
}
#pragma GCC diagnostic pop
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::CheckForReservedFile(const CommandContext &context, const string &filename)
{
#ifdef BUILD_STORAGE_DEVICE
    if (const auto [id, lun] = StorageDevice::GetIdsForReservedFile(filename); id != -1) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_IN_USE, filename,
            fmt::format("{0}:{1}", id, lun));
    }
#endif

    return true;
}
#pragma GCC diagnostic pop

string CommandExecutor::PrintCommand(const PbCommand &command, const PbDeviceDefinition &pb_device)
{
    const map<string, string, less<>> &params = { command.params().cbegin(), command.params().cend() };

    ostringstream s;
    s << "operation=" << PbOperation_Name(command.operation());

    if (!params.empty()) {
        s << ", command parameters=";
        bool isFirst = true;
        for (const auto& [key, value] : params) {
            if (!isFirst) {
                s << ", ";
            }
            isFirst = false;
            string v = key != "token" ? value : "????";
            s << "'" << key << "=" << v << "'";
        }
    }

    s << ", device=" << pb_device.id() << ":" << pb_device.unit();

    if (pb_device.type() != UNDEFINED) {
        s << ", type=" << PbDeviceType_Name(pb_device.type());
    }

    if (pb_device.params_size()) {
        s << ", device parameters=";
        bool isFirst = true;
        for (const auto& [key, value] : pb_device.params()) {
            if (!isFirst) {
                s << ":";
            }
            isFirst = false;
            s << "'" << key << "=" << value << "'";
        }
    }

    if (!pb_device.vendor().empty()) {
        s << ", vendor='" << pb_device.vendor() << '\'';
    }
    if (!pb_device.product().empty()) {
        s << ", product='" << pb_device.product() << '\'';
    }
    if (!pb_device.revision().empty()) {
        s << ", revision='" << pb_device.revision() << '\'';
    }

    if (pb_device.block_size()) {
        s << ", block size=" << pb_device.block_size();
    }

    if (pb_device.caching_mode() != PbCachingMode::DEFAULT) {
        s << ", caching mode=" << PbCachingMode_Name(pb_device.caching_mode());
    }

    return s.str();
}

bool CommandExecutor::EnsureLun0(const CommandContext &context, const PbCommand &command) const
{
    // Mapping of available LUNs (bit vector) to device IDs
    unordered_map<int32_t, int32_t> luns;

    // Collect LUN bit vectors of new devices
    for (const auto &device : command.devices()) {
        luns[device.id()] |= 1 << device.unit();
    }

    // Collect LUN bit vectors of existing devices
    for (const auto &device : controller_factory.GetAllDevices()) {
        luns[device->GetId()] |= 1 << device->GetLun();
    }

    const auto &it = ranges::find_if_not(luns, [](const auto &l) {return l.second & 0x01;});
    return
        it == luns.end() ?
            true : context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_LUN0, to_string((*it).first));
}

shared_ptr<PrimaryDevice> CommandExecutor::CreateDevice(const CommandContext &context,
    const PbDeviceDefinition &pb_device) const
{
    const string &filename = GetParam(pb_device, "file");

    auto device = DeviceFactory::GetInstance().CreateDevice(pb_device.type(), pb_device.unit(), filename);
    if (!device) {
        if (pb_device.type() == UNDEFINED) {
            context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, to_string(pb_device.id()),
                to_string(pb_device.unit()), filename);
        }
        else {
            context.ReturnLocalizedError(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, to_string(pb_device.id()),
                to_string(pb_device.unit()), PbDeviceType_Name(pb_device.type()));
        }

        return nullptr;
    }

    // SCDP must be attached only once
    if (device->GetType() == SCDP) {
        for (const auto &d : controller_factory.GetAllDevices()) {
            if (d->GetType() == device->GetType()) {
                context.ReturnLocalizedError(LocalizationKey::ERROR_UNIQUE_DEVICE_TYPE, GetTypeString(*device));
                return nullptr;
            }
        }
    }

    return device;
}

bool CommandExecutor::SetScsiLevel(const CommandContext &context, PrimaryDevice &device, int level) const
{
    if (level && !device.SetScsiLevel(static_cast<ScsiLevel>(level))) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SCSI_LEVEL, to_string(level));
    }

    return true;
}

bool CommandExecutor::SetBlockSize(const CommandContext &context, shared_ptr<PrimaryDevice> device,
    int block_size) const
{
#ifdef BUILD_STORAGE_DEVICE
    if (block_size) {
        if (device->SupportsImageFile()) {
            if (const auto storage_device = static_pointer_cast<StorageDevice>(device); !storage_device->SetConfiguredBlockSize(
                block_size)) {
                return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE, to_string(block_size));
            }
        }
        else {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
                GetTypeString(*device));
        }
    }

    return true;
#else
    return false;
#endif
}

bool CommandExecutor::ValidateOperation(const CommandContext &context, const PrimaryDevice &device)
{
    const PbOperation operation = context.GetCommand().operation();

    if ((operation == START || operation == STOP) && !device.IsStoppable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE,
            PbOperation_Name(operation), GetTypeString(device));
    }

    if ((operation == INSERT || operation == EJECT) && !device.IsRemovable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE,
            PbOperation_Name(operation), GetTypeString(device));
    }

    if ((operation == PROTECT || operation == UNPROTECT) && !device.IsProtectable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE,
            PbOperation_Name(operation), GetTypeString(device));
    }

    if ((operation == PROTECT || operation == UNPROTECT) && !device.IsReady()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_READY, PbOperation_Name(operation),
            GetTypeString(device));
    }

    return true;
}

bool CommandExecutor::ValidateDevice(const CommandContext &context, const PbDeviceDefinition &device) const
{
    const int id = device.id();
    if (id < 0) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_ID);
    }
    if (id >= 8) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_ID, to_string(id));
    }

    const int lun = device.unit();
    if (const int lun_max = GetLunMax(device.type()); lun < 0 || lun >= lun_max) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(lun_max - 1));
    }

    // For all commands except ATTACH the device and LUN must exist
    if (context.GetCommand().operation() == ATTACH) {
        return true;
    }

    if (!controller_factory.HasController(id)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_DEVICE, to_string(id));
    }

    if (!controller_factory.GetDeviceForIdAndLun(id, lun)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_UNIT, to_string(id), to_string(lun));
    }

    return true;
}

bool CommandExecutor::SetProductData(const CommandContext &context, const PbDeviceDefinition &pb_device,
    PrimaryDevice &device)
{
    const string &error = device.SetProductData( { pb_device.vendor(), pb_device.product(), pb_device.revision() },
        true);
    return error.empty() ? true : context.ReturnErrorStatus(error);
}

string CommandExecutor::GetTypeString(const Device &device)
{
    return PbDeviceType_Name(device.GetType());
}

string CommandExecutor::GetIdentifier(const Device &device)
{
    return fmt::format("{0} {1}:{2}", GetTypeString(device), device.GetId(), device.GetLun());
}
