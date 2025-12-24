//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
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
#include "protobuf/s2p_interface_util.h"
#include "shared/network_util.h"
#include "shared/s2p_exceptions.h"
#include "s2pctl_display.h"

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace network_util;
using namespace protobuf_util;
using namespace s2p_interface;
using namespace s2p_interface_util;
using namespace s2p_util;
using namespace s2pctl_display;

bool S2pCtlCommands::Execute(string_view log_level, string_view default_folder, string_view reserved_ids,
    string_view image_params, string_view filename)
{
    switch (command.operation()) {
    case LOG_LEVEL:
        return HandleLogLevel(log_level);

    case DEFAULT_FOLDER:
        return HandleDefaultImageFolder(default_folder);

    case RESERVE_IDS:
        return HandleReserveIds(reserved_ids);

    case CREATE_IMAGE:
        return HandleCreateImage(image_params);

    case DELETE_IMAGE:
        return HandleDeleteImage(image_params);

    case RENAME_IMAGE:
    case COPY_IMAGE:
        return HandleRenameCopyImage(image_params);

    case DEVICES_INFO:
        return HandleDeviceInfo();

    case DEVICE_TYPES_INFO:
        return HandleDeviceTypesInfo();

    case VERSION_INFO:
        return HandleVersionInfo();

    case SERVER_INFO:
        return HandleServerInfo();

    case DEFAULT_IMAGE_FILES_INFO:
        return HandleDefaultImageFilesInfo();

    case IMAGE_FILE_INFO:
        return HandleImageFileInfo(filename);

    case NETWORK_INTERFACES_INFO:
        return HandleNetworkInterfacesInfo();

    case LOG_LEVEL_INFO:
        return HandleLogLevelInfo();

    case RESERVED_IDS_INFO:
        return HandleReservedIdsInfo();

    case MAPPING_INFO:
        return HandleMappingInfo();

    case STATISTICS_INFO:
        return HandleStatisticsInfo();

    case PROPERTIES_INFO:
        return HandlePropertiesInfo();

    case OPERATION_INFO:
        return HandleOperationInfo();

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
        throw IoException("Can't resolve hostname '" + hostname + "'");
    }

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw IoException("Can't create socket: " + string(strerror(errno)));
    }

    server_addr.sin_port = htons(uint16_t(port));
    if (connect(fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(fd);

        throw IoException("Can't connect to s2p on host '" + hostname + "', port " + to_string(port)
            + ": " + strerror(errno));
    }

    if (array<uint8_t, 6> magic = { 'R', 'A', 'S', 'C', 'S', 'I' }; WriteBytes(fd, magic) != magic.size()) {
        close(fd);

        throw IoException("Can't write magic");
    }

    SerializeMessage(fd, command);
    DeserializeMessage(fd, result);

    close(fd);

    if (!result.status()) {
        throw IoException(result.msg());
    }

    if (!result.msg().empty()) {
        cout << result.msg() << '\n';
    }

    return true;
}

bool S2pCtlCommands::HandleDevicesInfo()
{
    SendCommand();

    cout << DisplayDevicesInfo(result.devices_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleLogLevel(string_view log_level)
{
    SetParam(command, "level", log_level);

    return SendCommand();
}

bool S2pCtlCommands::HandleReserveIds(string_view reserved_ids)
{
    SetParam(command, "ids", reserved_ids);

    return SendCommand();
}

bool S2pCtlCommands::HandleCreateImage(string_view image_params)
{
    if (!EvaluateParams(image_params, "file", "size")) {
        cerr << "Error: Invalid file descriptor '" << image_params << "', format is NAME:SIZE\n";

        return false;
    }

    SetParam(command, "read_only", "false");

    return SendCommand();
}

bool S2pCtlCommands::HandleDeleteImage(string_view filename)
{
    SetParam(command, "file", filename);

    return SendCommand();
}

bool S2pCtlCommands::HandleRenameCopyImage(string_view image_params)
{
    if (!EvaluateParams(image_params, "from", "to")) {
        cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME\n";

        return false;
    }

    return SendCommand();
}

bool S2pCtlCommands::HandleDefaultImageFolder(string_view folder)
{
    SetParam(command, "folder", folder);

    return SendCommand();
}

bool S2pCtlCommands::HandleDeviceInfo()
{
    SendCommand();

    for (const auto &device : result.devices_info().devices()) {
        cout << DisplayDeviceInfo(device);
    }

    cout << flush;

    return true;
}

bool S2pCtlCommands::HandleDeviceTypesInfo()
{
    SendCommand();

    cout << DisplayDeviceTypesInfo(result.device_types_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleVersionInfo()
{
    SendCommand();

    cout << DisplayVersionInfo(result.version_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleServerInfo()
{
    SendCommand();

    PbServerInfo server_info = result.server_info();

    if (server_info.has_version_info()) {
        cout << DisplayVersionInfo(server_info.version_info());
    }

    if (server_info.has_log_level_info()) {
        cout << DisplayLogLevelInfo(server_info.log_level_info());
    }

    if (server_info.has_image_files_info()) {
        cout << DisplayImageFilesInfo(server_info.image_files_info());
    }

    if (server_info.has_mapping_info()) {
        cout << DisplayMappingInfo(server_info.mapping_info());
    }

    if (server_info.has_network_interfaces_info()) {
        cout << DisplayNetworkInterfaces(server_info.network_interfaces_info());
    }

    if (server_info.has_device_types_info()) {
        cout << DisplayDeviceTypesInfo(server_info.device_types_info());
    }

    if (server_info.has_reserved_ids_info()) {
        cout << DisplayReservedIdsInfo(server_info.reserved_ids_info());
    }

    if (server_info.has_statistics_info()) {
        cout << DisplayStatisticsInfo(server_info.statistics_info());
    }

    if (server_info.has_properties_info()) {
        cout << DisplayPropertiesInfo(server_info.properties_info());
    }

    if (server_info.has_operation_info()) {
        cout << DisplayOperationInfo(server_info.operation_info());
    }

    if (server_info.has_devices_info() && server_info.devices_info().devices_size()) {
        vector<PbDevice> sorted_devices = { server_info.devices_info().devices().cbegin(),
            server_info.devices_info().devices().cend() };
        ranges::sort(sorted_devices, [](const auto &a, const auto &b) {return a.id() < b.id() || a.unit() < b.unit();});

        cout << "Attached devices:\n";

        for (const auto &device : sorted_devices) {
            cout << DisplayDeviceInfo(device);
        }
    }

    cout << flush;

    return true;
}

bool S2pCtlCommands::HandleDefaultImageFilesInfo()
{
    SendCommand();

    cout << DisplayImageFilesInfo(result.image_files_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleImageFileInfo(string_view filename)
{
    SetParam(command, "file", filename);

    SendCommand();

    cout << DisplayImageFile(result.image_file_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleNetworkInterfacesInfo()
{
    SendCommand();

    cout << DisplayNetworkInterfaces(result.network_interfaces_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleLogLevelInfo()
{
    SendCommand();

    cout << DisplayLogLevelInfo(result.log_level_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleReservedIdsInfo()
{
    SendCommand();

    cout << DisplayReservedIdsInfo(result.reserved_ids_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleMappingInfo()
{
    SendCommand();

    cout << DisplayMappingInfo(result.mapping_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleStatisticsInfo()
{
    SendCommand();

    cout << DisplayStatisticsInfo(result.statistics_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandlePropertiesInfo()
{
    SendCommand();

    cout << DisplayPropertiesInfo(result.properties_info()) << flush;

    return true;
}

bool S2pCtlCommands::HandleOperationInfo()
{
    SendCommand();

    cout << DisplayOperationInfo(result.operation_info()) << flush;

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
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (out.fail()) {
        throw IoException("Error: Can't create protobuf binary file '" + filename + "'");
    }
}

void S2pCtlCommands::ExportAsJson(const PbCommand &cmd, const string &filename) const
{
    string json;
    static_cast<void>(MessageToJsonString(cmd, &json));

    ofstream out(filename);
    out << json;
    if (out.fail()) {
        throw IoException("Can't create protobuf JSON file '" + filename + "'");
    }
}

void S2pCtlCommands::ExportAsText(const PbCommand &cmd, const string &filename) const
{
    string text;
    TextFormat::PrintToString(cmd, &text);

    ofstream out(filename);
    out << text;
    if (out.fail()) {
        throw IoException("Can't create protobuf text format file '" + filename + "'");
    }
}
