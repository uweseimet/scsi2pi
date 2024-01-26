//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "controllers/controller_factory.h"
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "protobuf/protobuf_util.h"
#include "command_dispatcher.h"

using namespace std;
using namespace spdlog;
using namespace s2p_interface;
using namespace s2p_util;
using namespace protobuf_util;

bool CommandDispatcher::DispatchCommand(const CommandContext &context, PbResult &result, const string &identifier)
{
    const PbCommand &command = context.GetCommand();
    const PbOperation operation = command.operation();

    if (!PbOperation_IsValid(operation)) {
        trace("Ignored unknown command with operation opcode " + to_string(operation));

        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION, UNKNOWN_OPERATION, to_string(operation));
    }

    trace("Executing {0} command", identifier, PbOperation_Name(operation));

    switch (operation) {
    case LOG_LEVEL:
        if (const string log_level = GetParam(command, "level"); !SetLogLevel(log_level)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL, log_level);
        }
        else {
            return context.ReturnSuccessStatus();
        }

    case DEFAULT_FOLDER:
        if (const string error = s2p_image.SetDefaultFolder(GetParam(command, "folder")); !error.empty()) {
            context.WriteResult(result);
            return false;
        }
        else {
            return context.WriteSuccessResult(result);
        }

    case DEVICES_INFO:
        response.GetDevicesInfo(executor.GetAllDevices(), result, command, s2p_image.GetDefaultFolder());
        return context.WriteSuccessResult(result);

    case DEVICE_TYPES_INFO:
        response.GetDeviceTypesInfo(*result.mutable_device_types_info());
        return context.WriteSuccessResult(result);

    case SERVER_INFO:
        response.GetServerInfo(*result.mutable_server_info(), command, executor.GetAllDevices(),
            executor.GetReservedIds(), s2p_image.GetDefaultFolder(), s2p_image.GetDepth());
        return context.WriteSuccessResult(result);

    case VERSION_INFO:
        response.GetVersionInfo(*result.mutable_version_info());
        return context.WriteSuccessResult(result);

    case LOG_LEVEL_INFO:
        response.GetLogLevelInfo(*result.mutable_log_level_info());
        return context.WriteSuccessResult(result);

    case DEFAULT_IMAGE_FILES_INFO:
        response.GetImageFilesInfo(*result.mutable_image_files_info(), s2p_image.GetDefaultFolder(),
            GetParam(command, "folder_pattern"), GetParam(command, "file_pattern"), s2p_image.GetDepth());
        return context.WriteSuccessResult(result);

    case IMAGE_FILE_INFO:
        if (string filename = GetParam(command, "file"); filename.empty()) {
            context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_FILENAME);
        }
        else {
            auto image_file = make_unique<PbImageFile>();
            const bool status = response.GetImageFile(*image_file.get(), s2p_image.GetDefaultFolder(),
                filename);
            if (status) {
                result.set_allocated_image_file_info(image_file.get());
                result.set_status(true);
                context.WriteResult(result);
            }
            else {
                context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_FILE_INFO);
            }
        }
        break;

    case NETWORK_INTERFACES_INFO:
        response.GetNetworkInterfacesInfo(*result.mutable_network_interfaces_info());
        return context.WriteSuccessResult(result);

    case MAPPING_INFO:
        response.GetMappingInfo(*result.mutable_mapping_info());
        return context.WriteSuccessResult(result);

    case STATISTICS_INFO:
        response.GetStatisticsInfo(*result.mutable_statistics_info(), executor.GetAllDevices());
        return context.WriteSuccessResult(result);

    case PROPERTIES_INFO:
        response.GetPropertiesInfo(*result.mutable_properties_info());
        return context.WriteSuccessResult(result);

    case OPERATION_INFO:
        response.GetOperationInfo(*result.mutable_operation_info(), s2p_image.GetDepth());
        return context.WriteSuccessResult(result);

    case RESERVED_IDS_INFO:
        response.GetReservedIds(*result.mutable_reserved_ids_info(), executor.GetReservedIds());
        return context.WriteSuccessResult(result);

    case SHUT_DOWN:
        return ShutDown(context, GetParam(command, "mode"));

    case NO_OPERATION:
        return context.ReturnSuccessStatus();

    case CREATE_IMAGE:
        return s2p_image.CreateImage(context);

    case DELETE_IMAGE:
        return s2p_image.DeleteImage(context);

    case RENAME_IMAGE:
        return s2p_image.RenameImage(context);

    case COPY_IMAGE:
        return s2p_image.CopyImage(context);

    case PROTECT_IMAGE:
        case UNPROTECT_IMAGE:
        return s2p_image.SetImagePermissions(context);

    case RESERVE_IDS:
        return executor.ProcessCmd(context);

    default:
        // The remaining commands may only be executed when the target is idle
        if (!ExecuteWithLock(context)) {
            return false;
        }

        return HandleDeviceListChange(context, operation);
    }

    return true;
}

bool CommandDispatcher::ExecuteWithLock(const CommandContext &context)
{
    scoped_lock<mutex> lock(executor.GetExecutionLocker());
    return executor.ProcessCmd(context);
}

bool CommandDispatcher::HandleDeviceListChange(const CommandContext &context, PbOperation operation) const
{
    // ATTACH and DETACH return the resulting device list
    if (operation == ATTACH || operation == DETACH) {
        // A command with an empty device list is required here in order to return data for all devices
        PbCommand command;
        PbResult result;
        response.GetDevicesInfo(executor.GetAllDevices(), result, command, s2p_image.GetDefaultFolder());
        context.WriteResult(result);
        return result.status();
    }

    return true;
}

// Shutdown on a remote interface command
bool CommandDispatcher::ShutDown(const CommandContext &context, const string &m) const
{
    if (m.empty()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING);
    }

    AbstractController::shutdown_mode mode = AbstractController::shutdown_mode::NONE;
    if (m == "rascsi") {
        mode = AbstractController::shutdown_mode::STOP_S2P;
    }
    else if (m == "system") {
        mode = AbstractController::shutdown_mode::STOP_PI;
    }
    else if (m == "reboot") {
        mode = AbstractController::shutdown_mode::RESTART_PI;
    }
    else {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, m);
    }

    // Shutdown modes other than rascsi require root permissions
    if (mode != AbstractController::shutdown_mode::STOP_S2P && getuid()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_PERMISSION);
    }

    // Report success now because after a shutdown nothing can be reported anymore
    PbResult result;
    context.WriteSuccessResult(result);

    return ShutDown(mode);
}

// Shutdown on a SCSI command
bool CommandDispatcher::ShutDown(AbstractController::shutdown_mode mode) const
{
    switch (mode) {
    case AbstractController::shutdown_mode::STOP_S2P:
        info("s2p shutdown requested");
        return true;

    case AbstractController::shutdown_mode::STOP_PI:
        info("Raspberry Pi shutdown requested");
        if (system("init 0") == -1) {
            error("Raspberry Pi shutdown failed");
        }
        break;

    case AbstractController::shutdown_mode::RESTART_PI:
        info("Raspberry Pi restart requested");
        if (system("init 6") == -1) {
            error("Raspberry Pi restart failed");
        }
        break;

    case AbstractController::shutdown_mode::NONE:
        assert(false);
        break;
    }

    return false;
}

bool CommandDispatcher::SetLogLevel(const string &log_level)
{
    int id = -1;
    int lun = -1;
    string level = log_level;

    if (const auto &components = Split(log_level, COMPONENT_SEPARATOR, 2); !components.empty()) {
        level = components[0];

        if (components.size() > 1) {
            if (const string error = ProcessId(ControllerFactory::GetIdMax(), ControllerFactory::GetLunMax(),
                components[1], id, lun); !error.empty()) {
                warn("Error setting log level: " + error);
                return false;
            }
        }
    }

    const level::level_enum l = level::from_str(level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != level) {
        warn("Invalid log level '" + level + "'");
        return false;
    }

    set_level(l);
    DeviceLogger::SetLogIdAndLun(id, lun);

    string msg;
    if (id != -1) {
        if (lun == -1) {
            msg = fmt::format("Set log level for device {0} to '{1}'", id, level);
        }
        else {
            msg = fmt::format("Set log level for device {0}:{1} to '{2}'", id, lun, level);
        }
    }
    else {
        msg = fmt::format("Set log level to '{}'", level);
    }
    info(msg);

    return true;
}
