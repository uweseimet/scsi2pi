//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_commands.h"
#include <fstream>
#include <iostream>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include "protobuf/protobuf_util.h"
#include "shared/network_util.h"
#include "shared/s2p_exceptions.h"

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace s2p_interface;
using namespace network_util;
using namespace s2p_util;
using namespace protobuf_util;

bool S2pCtlCommands::Execute(string_view log_level, string_view default_folder, string_view reserved_ids,
    string_view image_params, string_view filename)
{
    switch (command.operation()) {
    case LOG_LEVEL:
        return CommandLogLevel(log_level);

    case DEFAULT_FOLDER:
        return CommandDefaultImageFolder(default_folder);

    case RESERVE_IDS:
        return CommandReserveIds(reserved_ids);

    case CREATE_IMAGE:
        return CommandCreateImage(image_params);

    case DELETE_IMAGE:
        return CommandDeleteImage(image_params);

    case RENAME_IMAGE:
    case COPY_IMAGE:
        return CommandRenameCopyImage(image_params);

    case DEVICES_INFO:
        return CommandDeviceInfo();

    case DEVICE_TYPES_INFO:
        return CommandDeviceTypesInfo();

    case VERSION_INFO:
        return CommandVersionInfo();

    case SERVER_INFO:
        return CommandServerInfo();

    case DEFAULT_IMAGE_FILES_INFO:
        return CommandDefaultImageFilesInfo();

    case IMAGE_FILE_INFO:
        return CommandImageFileInfo(filename);

    case NETWORK_INTERFACES_INFO:
        return CommandNetworkInterfacesInfo();

    case LOG_LEVEL_INFO:
        return CommandLogLevelInfo();

    case RESERVED_IDS_INFO:
        return CommandReservedIdsInfo();

    case MAPPING_INFO:
        return CommandMappingInfo();

    case STATISTICS_INFO:
        return CommandStatisticsInfo();

    case PROPERTIES_INFO:
        return CommandPropertiesInfo();

    case OPERATION_INFO:
        return CommandOperationInfo();

    case NO_OPERATION:
        return false;

    default:
        return SendCommand();
    }

    return false;
}

bool S2pCtlCommands::SendCommand()
{
    if (!filename_binary.empty()) {
        ExportAsBinary(command, filename_binary);
    }
    if (!filename_json.empty()) {
        ExportAsJson(command, filename_json);
    }
    if (!filename_text.empty()) {
        ExportAsText(command, filename_text);
    }

    // Do not execute the command when the command data are exported
    if (!filename_binary.empty() || !filename_json.empty() || !filename_text.empty()) {
        return true;
    }

    sockaddr_in server_addr = { };
    if (!ResolveHostName(hostname, &server_addr)) {
        throw io_exception("Can't resolve hostname '" + hostname + "'");
    }

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw io_exception("Can't create socket: " + string(strerror(errno)));
    }

    server_addr.sin_port = htons(uint16_t(port));
    if (connect(fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(fd);

        throw io_exception("Can't connect to s2p on host '" + hostname + "', port " + to_string(port)
            + ": " + strerror(errno));
    }

    if (array<uint8_t, 6> magic = { 'R', 'A', 'S', 'C', 'S', 'I' }; WriteBytes(fd, magic) != magic.size()) {
        close(fd);

        throw io_exception("Can't write magic");
    }

    SerializeMessage(fd, command);
    DeserializeMessage(fd, result);

    close(fd);

    if (!result.status()) {
        throw io_exception(result.msg());
    }

    if (!result.msg().empty()) {
        cout << result.msg() << endl;
    }

    return true;
}

bool S2pCtlCommands::CommandDevicesInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayDevicesInfo(result.devices_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandLogLevel(string_view log_level)
{
    SetParam(command, "level", log_level);

    return SendCommand();
}

bool S2pCtlCommands::CommandReserveIds(string_view reserved_ids)
{
    SetParam(command, "ids", reserved_ids);

    return SendCommand();
}

bool S2pCtlCommands::CommandCreateImage(string_view image_params)
{
    if (!EvaluateParams(image_params, "file", "size")) {
        cerr << "Error: Invalid file descriptor '" << image_params << "', format is NAME:SIZE" << endl;

        return false;
    }

    SetParam(command, "read_only", "false");

    return SendCommand();
}

bool S2pCtlCommands::CommandDeleteImage(string_view filename)
{
    SetParam(command, "file", filename);

    return SendCommand();
}

bool S2pCtlCommands::CommandRenameCopyImage(string_view image_params)
{
    if (!EvaluateParams(image_params, "from", "to")) {
        cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;

        return false;
    }

    return SendCommand();
}

bool S2pCtlCommands::CommandDefaultImageFolder(string_view folder)
{
    SetParam(command, "folder", folder);

    return SendCommand();
}

bool S2pCtlCommands::CommandDeviceInfo()
{
    SendCommand();

    for (const auto &device : result.devices_info().devices()) {
        cout << s2pctl_display.DisplayDeviceInfo(device);
    }

    cout << flush;

    return true;
}

bool S2pCtlCommands::CommandDeviceTypesInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayDeviceTypesInfo(result.device_types_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandVersionInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayVersionInfo(result.version_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandServerInfo()
{
    SendCommand();

    PbServerInfo server_info = result.server_info();

    if (server_info.has_version_info()) {
        cout << s2pctl_display.DisplayVersionInfo(server_info.version_info());
    }

    if (server_info.has_log_level_info()) {
        cout << s2pctl_display.DisplayLogLevelInfo(server_info.log_level_info());
    }

    if (server_info.has_image_files_info()) {
        cout << s2pctl_display.DisplayImageFilesInfo(server_info.image_files_info());
    }

    if (server_info.has_mapping_info()) {
        cout << s2pctl_display.DisplayMappingInfo(server_info.mapping_info());
    }

    if (server_info.has_network_interfaces_info()) {
        cout << s2pctl_display.DisplayNetworkInterfaces(server_info.network_interfaces_info());
    }

    if (server_info.has_device_types_info()) {
        cout << s2pctl_display.DisplayDeviceTypesInfo(server_info.device_types_info());
    }

    if (server_info.has_reserved_ids_info()) {
        cout << s2pctl_display.DisplayReservedIdsInfo(server_info.reserved_ids_info());
    }

    if (server_info.has_statistics_info()) {
        cout << s2pctl_display.DisplayStatisticsInfo(server_info.statistics_info());
    }

    if (server_info.has_properties_info()) {
        cout << s2pctl_display.DisplayPropertiesInfo(server_info.properties_info());
    }

    if (server_info.has_operation_info()) {
        cout << s2pctl_display.DisplayOperationInfo(server_info.operation_info());
    }

    if (server_info.has_devices_info() && server_info.devices_info().devices_size()) {
        vector<PbDevice> sorted_devices = { server_info.devices_info().devices().cbegin(),
            server_info.devices_info().devices().cend() };
        ranges::sort(sorted_devices, [](const auto &a, const auto &b) {return a.id() < b.id() || a.unit() < b.unit();});

        cout << "Attached devices:\n";

        for (const auto &device : sorted_devices) {
            cout << s2pctl_display.DisplayDeviceInfo(device);
        }
    }

    cout << flush;

    return true;
}

bool S2pCtlCommands::CommandDefaultImageFilesInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayImageFilesInfo(result.image_files_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandImageFileInfo(string_view filename)
{
    SetParam(command, "file", filename);

    SendCommand();

    cout << s2pctl_display.DisplayImageFile(result.image_file_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandNetworkInterfacesInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayNetworkInterfaces(result.network_interfaces_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandLogLevelInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayLogLevelInfo(result.log_level_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandReservedIdsInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayReservedIdsInfo(result.reserved_ids_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandMappingInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayMappingInfo(result.mapping_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandStatisticsInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayStatisticsInfo(result.statistics_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandPropertiesInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayPropertiesInfo(result.properties_info()) << flush;

    return true;
}

bool S2pCtlCommands::CommandOperationInfo()
{
    SendCommand();

    cout << s2pctl_display.DisplayOperationInfo(result.operation_info()) << flush;

    return true;
}

bool S2pCtlCommands::EvaluateParams(string_view image_params, const string &key1, const string &key2)
{
    if (const auto &components = Split(string(image_params), COMPONENT_SEPARATOR, 2); components.size() == 2) {
        SetParam(command, key1, components[0]);
        SetParam(command, key2, components[1]);

        return true;
    }

    return false;
}

void S2pCtlCommands::ExportAsBinary(const PbCommand &cmd, const string &filename) const
{
    vector<uint8_t> data(cmd.ByteSizeLong());
    cmd.SerializeToArray(data.data(), static_cast<int>(data.size()));

    ofstream out(filename, ios::binary);
    out.write((const char*)data.data(), data.size());
    if (out.fail()) {
        throw io_exception("Error: Can't create protobuf binary file '" + filename + "'");
    }
}

void S2pCtlCommands::ExportAsJson(const PbCommand &cmd, const string &filename) const
{
    string json;
    (void)MessageToJsonString(cmd, &json);

    ofstream out(filename);
    out << json;
    if (out.fail()) {
        throw io_exception("Can't create protobuf JSON file '" + filename + "'");
    }
}

void S2pCtlCommands::ExportAsText(const PbCommand &cmd, const string &filename) const
{
    string text;
    TextFormat::PrintToString(cmd, &text);

    ofstream out(filename);
    out << text;
    if (out.fail()) {
        throw io_exception("Can't create protobuf text format file '" + filename + "'");
    }
}
