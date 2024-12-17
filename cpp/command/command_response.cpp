//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_response.h"
#include <spdlog/spdlog.h>
#include "base/device_factory.h"
#include "base/property_handler.h"
#include "command_context.h"
#include "command_image_support.h"
#include "controllers/controller.h"
#include "devices/disk.h"
#include "devices/scsi_generic.h"
#include "protobuf/protobuf_util.h"
#include "shared/network_util.h"
#include "shared/s2p_version.h"

using namespace spdlog;
using namespace s2p_util;
using namespace network_util;
using namespace protobuf_util;

void CommandResponse::GetDeviceProperties(shared_ptr<PrimaryDevice> device, PbDeviceProperties &properties) const
{
    properties.set_luns(Controller::GetLunMax(device->GetType() == SAHD));
    properties.set_scsi_level(static_cast<int>(device->GetScsiLevel()));
    properties.set_read_only(device->IsReadOnly());
    properties.set_protectable(device->IsProtectable());
    properties.set_stoppable(device->IsStoppable());
    properties.set_removable(device->IsRemovable());
    // All emulated removable media devices are lockable
    properties.set_lockable(device->IsRemovable());
    properties.set_supports_file(device->SupportsImageFile());
    properties.set_supports_params(device->SupportsParams());

    if (device->SupportsParams()) {
        for (const auto& [key, value] : device->GetDefaultParams()) {
            if (!value.empty()) {
                (*properties.mutable_default_params())[key] = value;
            }
        }
    }

#ifdef BUILD_STORAGE_DEVICE
    if (device->SupportsImageFile()) {
        const auto storage_device = static_pointer_cast<StorageDevice>(device);
        for (const auto &block_size : storage_device->GetSupportedBlockSizes()) {
            properties.add_block_sizes(block_size);
        }
    }
#endif
}

void CommandResponse::GetDeviceTypesInfo(PbDeviceTypesInfo &device_types_info) const
{
    int ordinal = 1;
    while (PbDeviceType_IsValid(ordinal)) {
        // Only report device types supported by the factory
        if (const auto device = DeviceFactory::Instance().CreateDevice(static_cast<PbDeviceType>(ordinal), 0, ""); device) {
            auto type_properties = device_types_info.add_properties();
            type_properties->set_type(device->GetType());
            GetDeviceProperties(device, *type_properties->mutable_properties());
        }

        ++ordinal;
    }
}

void CommandResponse::GetDevice(shared_ptr<PrimaryDevice> device, PbDevice &pb_device) const
{
    pb_device.set_id(device->GetId());
    pb_device.set_unit(device->GetLun());
    const auto &product_data = device->GetProductData();
    pb_device.set_vendor(product_data.vendor);
    pb_device.set_product(product_data.product);
    pb_device.set_revision(product_data.revision);
    pb_device.set_type(device->GetType());
    pb_device.set_scsi_level(static_cast<int>(device->GetScsiLevel()));

    GetDeviceProperties(device, *pb_device.mutable_properties());

    auto status = pb_device.mutable_status();
    status->set_protected_(device->IsProtected());
    status->set_stopped(device->IsStopped());
    status->set_removed(device->IsRemoved());
    status->set_locked(device->IsLocked());

    if (device->SupportsParams()) {
        for (const auto& [key, value] : device->GetParams()) {
            SetParam(pb_device, key, value);
        }
    }

#ifdef BUILD_STORAGE_DEVICE
    if (device->SupportsImageFile()) {
        const auto storage_device = static_pointer_cast<const StorageDevice>(device);
        pb_device.set_block_size(storage_device->IsRemoved() ? 0 : storage_device->GetBlockSize());
        pb_device.set_block_count(storage_device->IsRemoved() ? 0 : storage_device->GetBlockCount());
        GetImageFile(*pb_device.mutable_file(), storage_device->IsReady() ? storage_device->GetFilename() : "");
    }
#endif
#ifdef BUILD_DISK
    if (const auto disk = dynamic_pointer_cast<const Disk>(device); disk) {
        pb_device.set_caching_mode(disk->GetCachingMode());
    }
#endif
#ifdef BUILD_SCSG
    if (const auto sg = dynamic_pointer_cast<const ScsiGeneric>(device); sg) {
        pb_device.mutable_file()->set_name(sg->GetDevice());
        pb_device.set_type(SCSG);
    }
#endif
}

bool CommandResponse::GetImageFile(PbImageFile &image_file, const string &filename) const
{
    if (!filename.empty()) {
        image_file.set_name(filename);
        image_file.set_type(DeviceFactory::Instance().GetTypeForFile(filename));

        const path p(filename[0] == '/' ? filename : CommandImageSupport::Instance().GetDefaultFolder() + "/" + filename);

        image_file.set_read_only(access(p.c_str(), W_OK));

        error_code error;
        if (is_regular_file(p, error) || (is_symlink(p, error) && !is_block_file(p, error))) {
            image_file.set_size(file_size(p));
            return true;
        }
    }

    return false;
}

void CommandResponse::GetAvailableImages(PbImageFilesInfo &image_files_info, const string &folder_pattern,
    const string &file_pattern) const
{
    const string &default_folder = CommandImageSupport::Instance().GetDefaultFolder();

    const path default_path(default_folder);
    if (!is_directory(default_path)) {
        return;
    }

    const string folder_pattern_lower = ToLower(folder_pattern);
    const string file_pattern_lower = ToLower(file_pattern);

    for (auto it = recursive_directory_iterator(default_path, directory_options::follow_directory_symlink);
        it != recursive_directory_iterator(); it++) {
        if (it.depth() > CommandImageSupport::Instance().GetDepth()) {
            it.disable_recursion_pending();
            continue;
        }

        const string parent = it->path().parent_path().string();

        const string folder = parent.size() > default_folder.size() ? parent.substr(default_folder.size() + 1) : "";

        if (!FilterMatches(folder, folder_pattern_lower)
            || !FilterMatches(it->path().filename().string(), file_pattern_lower)) {
            continue;
        }

        if (!ValidateImageFile(it->path())) {
            continue;
        }

        const string filename = folder.empty() ?
                                                 it->path().filename().string() :
                                                 folder + "/" + it->path().filename().string();
        if (PbImageFile image_file; GetImageFile(image_file, filename)) {
            GetImageFile(*image_files_info.add_image_files(), filename);
        }
    }
}

void CommandResponse::GetImageFilesInfo(PbImageFilesInfo &image_files_info, const string &folder_pattern,
    const string &file_pattern) const
{
    image_files_info.set_default_image_folder(CommandImageSupport::Instance().GetDefaultFolder());
    image_files_info.set_depth(CommandImageSupport::Instance().GetDepth());

    GetAvailableImages(image_files_info, folder_pattern, file_pattern);
}

void CommandResponse::GetAvailableImages(PbServerInfo &server_info, const string &folder_pattern,
    const string &file_pattern) const
{
    server_info.mutable_image_files_info()->set_default_image_folder(CommandImageSupport::Instance().GetDefaultFolder());

    GetImageFilesInfo(*server_info.mutable_image_files_info(), folder_pattern, file_pattern);
}

void CommandResponse::GetReservedIds(PbReservedIdsInfo &reserved_ids_info, const unordered_set<int> &ids) const
{
    for (const int id : ids) {
        reserved_ids_info.add_ids(id);
    }
}

void CommandResponse::GetDevices(const unordered_set<shared_ptr<PrimaryDevice>> &devices,
    PbServerInfo &server_info) const
{
    for (const auto &device : devices) {
        PbDevice *pb_device = server_info.mutable_devices_info()->add_devices();
        GetDevice(device, *pb_device);
    }
}

void CommandResponse::GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>> &devices, PbResult &result,
    const PbCommand &command) const
{
    set<id_set> id_sets;

    // If no device list was provided in the command get information on all devices
    if (!command.devices_size()) {
        for (const auto &device : devices) {
            id_sets.insert( { device->GetId(), device->GetLun() });
        }
    }
    // Otherwise get information on the devices provided in the command
    else {
        id_sets = MatchDevices(devices, result, command);
        if (id_sets.empty()) {
            return;
        }
    }

    for (const auto& [i, l] : id_sets) {
        // Work-around for old compilers that have issues with directly referencing id/lun in the lambda below
        const int id = i;
        const int lun = l;
        if (const auto &it = ranges::find_if(devices,
            [&id, &lun](const auto &d) {return d->GetId() == id && d->GetLun() == lun;}); it != devices.end()) {
            GetDevice(*it, *result.mutable_devices_info()->add_devices());
        }
    }

    result.set_status(true);
}

void CommandResponse::GetServerInfo(PbServerInfo &server_info, const PbCommand &command,
    const unordered_set<shared_ptr<PrimaryDevice>> &devices, const unordered_set<int> &reserved_ids) const
{
    const auto &command_operations = Split(GetParam(command, "operations"), ',');
    set<string, less<>> operations;
    for (const string &operation : command_operations) {
        operations.insert(ToUpper(operation));
    }

    if (!operations.empty()) {
        CreateLogger(CommandContext::LOGGER_NAME)->trace("Requested operation(s): " + Join(operations, ","));
    }

    if (HasOperation(operations, PbOperation::VERSION_INFO)) {
        GetVersionInfo(*server_info.mutable_version_info());
    }

    if (HasOperation(operations, PbOperation::LOG_LEVEL_INFO)) {
        GetLogLevelInfo(*server_info.mutable_log_level_info());
    }

    if (HasOperation(operations, PbOperation::DEVICE_TYPES_INFO)) {
        GetDeviceTypesInfo(*server_info.mutable_device_types_info());
    }

    if (HasOperation(operations, PbOperation::DEFAULT_IMAGE_FILES_INFO)) {
        GetAvailableImages(server_info, GetParam(command, "folder_pattern"), GetParam(command, "file_pattern"));
    }

    if (HasOperation(operations, PbOperation::NETWORK_INTERFACES_INFO)) {
        GetNetworkInterfacesInfo(*server_info.mutable_network_interfaces_info());
    }

    if (HasOperation(operations, PbOperation::MAPPING_INFO)) {
        GetMappingInfo(*server_info.mutable_mapping_info());
    }

    if (HasOperation(operations, PbOperation::STATISTICS_INFO)) {
        GetStatisticsInfo(*server_info.mutable_statistics_info(), devices);
    }

    if (HasOperation(operations, PbOperation::PROPERTIES_INFO)) {
        GetPropertiesInfo(*server_info.mutable_properties_info());
    }

    if (HasOperation(operations, PbOperation::DEVICES_INFO)) {
        GetDevices(devices, server_info);
    }

    if (HasOperation(operations, PbOperation::RESERVED_IDS_INFO)) {
        GetReservedIds(*server_info.mutable_reserved_ids_info(), reserved_ids);
    }

    if (HasOperation(operations, PbOperation::OPERATION_INFO)) {
        GetOperationInfo(*server_info.mutable_operation_info());
    }
}

void CommandResponse::GetVersionInfo(PbVersionInfo &version_info) const
{
    version_info.set_major_version(s2p_major_version);
    version_info.set_minor_version(s2p_minor_version);
    version_info.set_patch_version(s2p_revision);
    version_info.set_suffix(s2p_suffix);
    version_info.set_identifier("SCSI2Pi");
}

void CommandResponse::GetLogLevelInfo(PbLogLevelInfo &log_level_info) const
{
    for (const auto &log_level : level::level_string_views) {
        log_level_info.add_log_levels(log_level.data());
    }

    log_level_info.set_current_log_level(level::level_string_views[get_level()].data());
}

void CommandResponse::GetNetworkInterfacesInfo(PbNetworkInterfacesInfo &network_interfaces_info) const
{
    for (const auto &network_interface : GetNetworkInterfaces()) {
        network_interfaces_info.add_name(network_interface);
    }
}

void CommandResponse::GetMappingInfo(PbMappingInfo &mapping_info) const
{
    for (const auto& [name, type] : DeviceFactory::Instance().GetExtensionMapping()) {
        (*mapping_info.mutable_mapping())[name] = type;
    }
}

void CommandResponse::GetStatisticsInfo(PbStatisticsInfo &statistics_info,
    const unordered_set<shared_ptr<PrimaryDevice>> &devices) const
{
    for (const auto &device : devices) {
        for (const auto &statistics : device->GetStatistics()) {
            auto s = statistics_info.add_statistics();
            s->set_id(statistics.id());
            s->set_unit(statistics.unit());
            s->set_category(statistics.category());
            s->set_key(statistics.key());
            s->set_value(statistics.value());
        }
    }
}

void CommandResponse::GetPropertiesInfo(PbPropertiesInfo &properties_info) const
{
    for (const auto& [key, value] : PropertyHandler::Instance().GetProperties()) {
        (*properties_info.mutable_s2p_properties())[key] = value;
    }
}

void CommandResponse::GetOperationInfo(PbOperationInfo &operation_info) const
{
    auto operation = CreateOperation(operation_info, ATTACH, "Attach device, device-specific parameters are required");
    AddOperationParameter(*operation, "name", "Image file name in case of a mass storage device");
    AddOperationParameter(*operation, "interface", "Comma-separated prioritized network interface list");
    AddOperationParameter(*operation, "inet", "IP address and netmask of the network bridge");
    AddOperationParameter(*operation, "cmd", "Print command for the printer device");

    CreateOperation(operation_info, DETACH, "Detach device, device-specific parameters are required");

    CreateOperation(operation_info, DETACH_ALL, "Detach all devices");

    CreateOperation(operation_info, START, "Start device, device-specific parameters are required");

    CreateOperation(operation_info, STOP, "Stop device, device-specific parameters are required");

    operation = CreateOperation(operation_info, INSERT, "Insert medium, device-specific parameters are required");
    AddOperationParameter(*operation, "file", "Image file name", "", true);

    CreateOperation(operation_info, EJECT, "Eject medium, device-specific parameters are required");

    CreateOperation(operation_info, PROTECT, "Protect medium, device-specific parameters are required");

    CreateOperation(operation_info, UNPROTECT, "Unprotect medium, device-specific parameters are required");

    operation = CreateOperation(operation_info, SERVER_INFO, "Get server information");
    if (CommandImageSupport::Instance().GetDepth()) {
        AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names");
    }
    AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names");

    CreateOperation(operation_info, VERSION_INFO, "Get device server version");

    CreateOperation(operation_info, DEVICES_INFO, "Get information on attached devices");

    CreateOperation(operation_info, DEVICE_TYPES_INFO, "Get device properties by device type");

    operation = CreateOperation(operation_info, DEFAULT_IMAGE_FILES_INFO, "Get information on available image files");
    if (CommandImageSupport::Instance().GetDepth()) {
        AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names");
    }
    AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names");

    operation = CreateOperation(operation_info, IMAGE_FILE_INFO, "Get information on image file");
    AddOperationParameter(*operation, "file", "Image file name", "", true);

    CreateOperation(operation_info, LOG_LEVEL_INFO, "Get log level information");

    CreateOperation(operation_info, NETWORK_INTERFACES_INFO, "Get the available network interfaces");

    CreateOperation(operation_info, MAPPING_INFO, "Get mapping of extensions to device types");

    CreateOperation(operation_info, STATISTICS_INFO, "Get statistics");

    CreateOperation(operation_info, RESERVED_IDS_INFO, "Get list of reserved device IDs");

    operation = CreateOperation(operation_info, DEFAULT_FOLDER, "Set default image file folder");
    AddOperationParameter(*operation, "folder", "Default image file folder name", "", true);

    operation = CreateOperation(operation_info, LOG_LEVEL, "Set log level");
    AddOperationParameter(*operation, "level", "New log level", "", true);

    operation = CreateOperation(operation_info, RESERVE_IDS, "Reserve device IDs");
    AddOperationParameter(*operation, "ids", "Comma-separated device ID list", "", true);

    operation = CreateOperation(operation_info, SHUT_DOWN, "Shut down or reboot");
    if (getuid()) {
        AddOperationParameter(*operation, "mode", "Shutdown mode", "", true, { "rascsi" });
    }
    else {
        // System shutdown/reboot requires root permissions
        AddOperationParameter(*operation, "mode", "Shutdown mode", "", true, { "rascsi", "system", "reboot" });
    }

    operation = CreateOperation(operation_info, CREATE_IMAGE, "Create an image file");
    AddOperationParameter(*operation, "file", "Image file name", "", true);
    AddOperationParameter(*operation, "size", "Image file size in bytes", "", true);
    AddOperationParameter(*operation, "read_only", "Read-only flag", "false", false, { "true", "false" });

    operation = CreateOperation(operation_info, DELETE_IMAGE, "Delete image file");
    AddOperationParameter(*operation, "file", "Image file name", "", true);

    operation = CreateOperation(operation_info, RENAME_IMAGE, "Rename image file");
    AddOperationParameter(*operation, "from", "Source image file name", "", true);
    AddOperationParameter(*operation, "to", "Destination image file name", "", true);

    operation = CreateOperation(operation_info, COPY_IMAGE, "Copy image file");
    AddOperationParameter(*operation, "from", "Source image file name", "", true);
    AddOperationParameter(*operation, "to", "Destination image file name", "", true);
    AddOperationParameter(*operation, "read_only", "Read-only flag", "false", false, { "true", "false" });

    operation = CreateOperation(operation_info, PROTECT_IMAGE, "Write-protect image file");
    AddOperationParameter(*operation, "file", "Image file name", "", true);

    operation = CreateOperation(operation_info, UNPROTECT_IMAGE, "Make image file writable");
    AddOperationParameter(*operation, "file", "Image file name", "", true);

    operation = CreateOperation(operation_info, CHECK_AUTHENTICATION, "Check whether an authentication token is valid");
    AddOperationParameter(*operation, "token", "Authentication token to be checked", "", true);

    CreateOperation(operation_info, PROPERTIES_INFO, "Get current s2p properties");

    CreateOperation(operation_info, PERSIST_CONFIGURATION, "Save current configuration to /etc/s2p.conf");

    CreateOperation(operation_info, OPERATION_INFO, "Get operation meta data");
}

// This method returns a raw pointer because protobuf does not have support for smart pointers
PbOperationMetaData* CommandResponse::CreateOperation(PbOperationInfo &operation_info, const PbOperation &operation,
    const string &description) const
{
    PbOperationMetaData meta_data;
    meta_data.set_server_side_name(PbOperation_Name(operation));
    meta_data.set_description(description);
    const int number = PbOperation_descriptor()->FindValueByName(PbOperation_Name(operation))->number();
    (*operation_info.mutable_operations())[number] = meta_data;
    return &(*operation_info.mutable_operations())[number];
}

void CommandResponse::AddOperationParameter(PbOperationMetaData &meta_data, const string &name,
    const string &description, const string &default_value, bool is_mandatory,
    const vector<string> &permitted_values) const
{
    auto parameter = meta_data.add_parameters();
    parameter->set_name(name);
    parameter->set_description(description);
    parameter->set_default_value(default_value);
    parameter->set_is_mandatory(is_mandatory);
    for (const auto &permitted_value : permitted_values) {
        parameter->add_permitted_values(permitted_value);
    }
}

set<id_set> CommandResponse::MatchDevices(const unordered_set<shared_ptr<PrimaryDevice>> &devices, PbResult &result,
    const PbCommand &command) const
{
    set<id_set> id_sets;

    for (const auto &device : command.devices()) {
        bool has_device = false;
        if (ranges::any_of(devices,
            [&device](const auto &d) {return d->GetId() == device.id() && d->GetLun() == device.unit();})) {
            id_sets.insert( { device.id(), device.unit() });
            has_device = true;
        }

        if (!has_device) {
            id_sets.clear();

            result.set_status(false);
            result.set_msg("No device for " + to_string(device.id()) + ":" + to_string(device.unit()));

            break;
        }
    }

    return id_sets;
}

bool CommandResponse::ValidateImageFile(const path &path)
{
    if (path.filename().string().starts_with(".")) {
        return false;
    }

    filesystem::path p(path);

    // Follow symlink
    if (is_symlink(p)) {
        p = read_symlink(p);
        if (!exists(p)) {
            CreateLogger(CommandContext::LOGGER_NAME)->warn("Image file symlink '{}' is broken", path.string());
            return false;
        }
    }

    if (is_directory(p) || (is_other(p) && !is_block_file(p))) {
        return false;
    }

    if (!is_block_file(p) && file_size(p) < 256) {
        CreateLogger(CommandContext::LOGGER_NAME)->warn("Image file '{}' is invalid", p.string());
        return false;
    }

    return true;
}

bool CommandResponse::FilterMatches(const string &input, string_view pattern_lower)
{
    return pattern_lower.empty() || ToLower(input).find(pattern_lower) != string::npos;
}

bool CommandResponse::HasOperation(const set<string, less<>> &operations, PbOperation operation)
{
    return operations.empty() || operations.contains(PbOperation_Name(operation));
}
