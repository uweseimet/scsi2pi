//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_dispatcher.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include "command_context.h"
#include "command_image_support.h"
#include "command_response.h"
#include "controllers/controller_factory.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace s2p_util;
using namespace protobuf_util;

bool CommandDispatcher::DispatchCommand(const CommandContext &context, PbResult &result)
{
    const PbCommand &command = context.GetCommand();
    const PbOperation operation = command.operation();

    if (!PbOperation_IsValid(operation)) {
        trace("Ignored unknown command with operation opcode {}", static_cast<int>(operation));

        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION, UNKNOWN_OPERATION,
            to_string(static_cast<int>(operation)));
    }

    trace("Executing {} command", PbOperation_Name(operation));

    CommandResponse response;

    switch (operation) {
    case LOG_LEVEL:
        if (const string &log_level = GetParam(command, "level"); !SetLogLevel(log_level)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL, log_level);
        }
        else {
            PropertyHandler::Instance().AddProperty(PropertyHandler::LOG_LEVEL, log_level);
            return context.ReturnSuccessStatus();
        }

    case DEFAULT_FOLDER:
        if (const string &error = CommandImageSupport::Instance().SetDefaultFolder(GetParam(command, "folder")); !error.empty()) {
            result.set_msg(error);
            return context.WriteResult(result);
        }
        else {
            PropertyHandler::Instance().AddProperty(PropertyHandler::IMAGE_FOLDER, GetParam(command, "folder"));
            return context.WriteSuccessResult(result);
        }

    case DEVICES_INFO:
        response.GetDevicesInfo(executor.GetAllDevices(), result, command);
        return context.WriteSuccessResult(result);

    case DEVICE_TYPES_INFO:
        response.GetDeviceTypesInfo(*result.mutable_device_types_info());
        return context.WriteSuccessResult(result);

    case SERVER_INFO:
        response.GetServerInfo(*result.mutable_server_info(), command, executor.GetAllDevices(),
            executor.GetReservedIds());
        return context.WriteSuccessResult(result);

    case VERSION_INFO:
        response.GetVersionInfo(*result.mutable_version_info());
        return context.WriteSuccessResult(result);

    case LOG_LEVEL_INFO:
        response.GetLogLevelInfo(*result.mutable_log_level_info());
        return context.WriteSuccessResult(result);

    case DEFAULT_IMAGE_FILES_INFO:
        response.GetImageFilesInfo(*result.mutable_image_files_info(), GetParam(command, "folder_pattern"),
            GetParam(command, "file_pattern"));
        return context.WriteSuccessResult(result);

    case IMAGE_FILE_INFO:
        if (const string &filename = GetParam(command, "file"); filename.empty()) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_FILENAME);
        }
        else {
            if (const auto &image_file = make_unique<PbImageFile>(); response.GetImageFile(*image_file.get(),
                filename)) {
                result.set_allocated_image_file_info(image_file.get());
                result.set_status(true);
                return context.WriteResult(result);
            }
            else {
                return context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_FILE_INFO, filename);
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
        response.GetOperationInfo(*result.mutable_operation_info());
        return context.WriteSuccessResult(result);

    case RESERVED_IDS_INFO:
        response.GetReservedIds(*result.mutable_reserved_ids_info(), executor.GetReservedIds());
        return context.WriteSuccessResult(result);

    case SHUT_DOWN:
        return ShutDown(context);

    case CREATE_IMAGE:
        return CommandImageSupport::Instance().CreateImage(context);

    case DELETE_IMAGE:
        return CommandImageSupport::Instance().DeleteImage(context);

    case RENAME_IMAGE:
        return CommandImageSupport::Instance().RenameImage(context);

    case COPY_IMAGE:
        return CommandImageSupport::Instance().CopyImage(context);

    case PROTECT_IMAGE:
    case UNPROTECT_IMAGE:
        return CommandImageSupport::Instance().SetImagePermissions(context);

    case PERSIST_CONFIGURATION:
        return PropertyHandler::Instance().Persist() ?
                context.ReturnSuccessStatus() : context.ReturnLocalizedError(LocalizationKey::ERROR_PERSIST);

    case NO_OPERATION:
        return context.ReturnSuccessStatus();

    default:
        // The remaining commands may only be executed when the target is idle
        return ExecuteWithLock(context) ? HandleDeviceListChange(context) : false;
    }

    return true;
}

bool CommandDispatcher::ExecuteWithLock(const CommandContext &context)
{
    scoped_lock<mutex> lock(executor.GetExecutionLocker());
    return executor.ProcessCmd(context);
}

bool CommandDispatcher::HandleDeviceListChange(const CommandContext &context) const
{
    // ATTACH and DETACH return the resulting device list
    if (const PbOperation operation = context.GetCommand().operation(); operation == ATTACH || operation == DETACH) {
        // A command with an empty device list is required here in order to return data for all devices
        PbCommand command;
        PbResult result;
        CommandResponse response;
        response.GetDevicesInfo(executor.GetAllDevices(), result, command);
        return context.WriteResult(result);
    }

    return true;
}

// Shutdown on a remote interface command
bool CommandDispatcher::ShutDown(const CommandContext &context) const
{
    shutdown_mode mode = shutdown_mode::none;

    if (const string &m = GetParam(context.GetCommand(), "mode"); m == "rascsi") {
        mode = shutdown_mode::stop_s2p;
    }
    else if (m == "system") {
        mode = shutdown_mode::stop_pi;
    }
    else if (m == "reboot") {
        mode = shutdown_mode::restart_pi;
    }
    else {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, m);
    }

    // Shutdown modes other than "rascsi" require root permissions
    if (mode != shutdown_mode::stop_s2p && getuid()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_PERMISSION);
    }

    // Report success now because after a shutdown nothing can be reported anymore
    PbResult result;
    context.WriteSuccessResult(result);

    return ShutDown(mode);
}

// Shutdown on a SCSI command
bool CommandDispatcher::ShutDown(shutdown_mode mode) const
{
    switch (mode) {
    case shutdown_mode::stop_s2p:
        info("s2p shutdown requested");
        return true;

    case shutdown_mode::stop_pi:
        info("Pi shutdown requested");
        (void)system("init 0");
        error("Pi shutdown failed");
        break;

    case shutdown_mode::restart_pi:
        info("Pi restart requested");
        (void)system("init 6");
        error("Pi restart failed");
        break;

    default:
        error("Invalid shutdown mode {}", static_cast<int>(mode));
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
            if (const string &error = ProcessId(components[1], id, lun); !error.empty()) {
                warn("Error setting log level: " + error);
                return false;
            }
        }
    }

    const level::level_enum l = level::from_str(level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != level) {
        warn("Invalid log level '{}'", level);
        return false;
    }

    set_level(l);
    DeviceLogger::SetLogIdAndLun(id, lun);

    if (id != -1) {
        if (lun == -1) {
            info("Set log level for device {0} to '{1}'", id, level);
        }
        else {
            info("Set log level for device {0}:{1} to '{2}'", id, lun, level);
        }
    }
    else {
        info("Set log level to '{}'", level);
    }

    return true;
}
