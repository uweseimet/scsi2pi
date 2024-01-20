//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <sstream>
#include <array>
#include <vector>
#include <iomanip>
#include <spdlog/spdlog.h>
#include "shared/shared_exceptions.h"
#include "shared/s2p_util.h"
#include "protobuf_util.h"

using namespace std;
using namespace s2p_util;
using namespace s2p_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

void protobuf_util::ParseParameters(PbDeviceDefinition &device, const string &params)
{
    if (params.empty()) {
        return;
    }

    // Old style parameter (filename only), for backwards compatibility and convenience
    if (params.find(KEY_VALUE_SEPARATOR) == string::npos) {
        SetParam(device, "file", params);
        return;
    }

    for (const auto &p : Split(params, COMPONENT_SEPARATOR)) {
        if (const auto &param = Split(p, KEY_VALUE_SEPARATOR, 2); param.size() == 2) {
            SetParam(device, param[0], param[1]);
        }
    }
}

string protobuf_util::SetCommandParams(PbCommand &command, const string &params)
{
    if (params.find(KEY_VALUE_SEPARATOR) != string::npos) {
        return SetFromGenericParams(command, params);
    }

    string folder_pattern;
    string file_pattern;
    string operations;

    switch (const auto &components = Split(params, COMPONENT_SEPARATOR, 3); components.size()) {
    case 3:
        operations = components[2];
        [[fallthrough]];

    case 2:
        folder_pattern = components[0];
        file_pattern = components[1];
        break;

    case 1:
        file_pattern = components[0];
        break;

    default:
        assert(false);
        break;
    }

    SetParam(command, "folder_pattern", folder_pattern);
    SetParam(command, "file_pattern", file_pattern);
    SetParam(command, "operations", operations);

    return "";
}

string protobuf_util::SetFromGenericParams(PbCommand &command, const string &params)
{
    for (const string &key_value : Split(params, COMPONENT_SEPARATOR)) {
        const auto &param = Split(key_value, KEY_VALUE_SEPARATOR, 2);
        if (param.size() > 1 && !param[0].empty()) {
            SetParam(command, param[0], param[1]);
        }
        else {
            return "Parameter '" + key_value + "' has to be a key/value pair";
        }
    }

    return "";
}

void protobuf_util::SetProductData(PbDeviceDefinition &device, const string &data)
{
    const auto &components = Split(data, COMPONENT_SEPARATOR, 3);
    switch (components.size()) {
    case 3:
        device.set_revision(components[2]);
        [[fallthrough]];

    case 2:
        device.set_product(components[1]);
        [[fallthrough]];

    case 1:
        device.set_vendor(components[0]);
        break;

    default:
        break;
    }
}

string protobuf_util::SetIdAndLun(int id_max, int lun_max, PbDeviceDefinition &device, const string &value)
{
    int id;
    int lun;
    if (const string error = ProcessId(id_max, lun_max, value, id, lun); !error.empty()) {
        return error;
    }

    device.set_id(id);
    device.set_unit(lun != -1 ? lun : 0);

    return "";
}

string protobuf_util::ListDevices(const vector<PbDevice> &pb_devices)
{
    if (pb_devices.empty()) {
        return "No devices currently attached.\n";
    }

    ostringstream s;
    s << "+----+-----+------+-------------------------------------\n"
        << "| ID | LUN | TYPE | IMAGE FILE\n"
        << "+----+-----+------+-------------------------------------\n";

    vector<PbDevice> devices = pb_devices;
    ranges::sort(devices, [](const auto &a, const auto &b) {return a.id() < b.id() || a.unit() < b.unit();});

    for (const auto &device : devices) {
        string filename;
        switch (device.type()) {
        case SCDP:
            filename = "DaynaPort SCSI/Link";
            break;

        case SCHS:
            filename = "Host Services";
            break;

        case SCLP:
            filename = "SCSI Printer";
            break;

        default:
            filename = device.file().name();
            break;
        }

        s << "|  " << device.id() << " | " << setw(3) << device.unit() << " | " << PbDeviceType_Name(device.type())
            << " | "
            << (filename.empty() ? "NO MEDIUM" : filename)
            << (
            !device.status().removed() && (device.properties().read_only() || device.status().protected_()) ?
                " (READ-ONLY)" : "")
            << '\n';
    }

    s << "+----+-----+------+-------------------------------------\n";

    return s.str();
}

//---------------------------------------------------------------------------
//
// Serialize/Deserialize protobuf message: Length followed by the actual data.
// A little endian platform is assumed.
//
//---------------------------------------------------------------------------

void protobuf_util::SerializeMessage(int fd, const google::protobuf::Message &message)
{
    const string s = message.SerializeAsString();
    vector<uint8_t> data(s.begin(), s.end());

    // Write the size of the protobuf data as a header
    if (array<uint8_t, 4> header = { static_cast<uint8_t>(data.size()), static_cast<uint8_t>(data.size() >> 8),
        static_cast<uint8_t>(data.size() >> 16), static_cast<uint8_t>(data.size() >> 24) };
    WriteBytes(fd, header) != header.size()) {
        throw io_exception("Can't write message size: " + string(strerror(errno)));
    }

    // Write the actual protobuf data
    if (WriteBytes(fd, data) != data.size()) {
        throw io_exception("Can't write message data: " + string(strerror(errno)));
    }
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::Message &message)
{
    // Read the header with the size of the protobuf data
    array<byte, 4> header;
    if (ReadBytes(fd, header) < header.size()) {
        throw io_exception("Can't read message size: " + string(strerror(errno)));
    }

    const int size = (static_cast<int>(header[3]) << 24) + (static_cast<int>(header[2]) << 16)
        + (static_cast<int>(header[1]) << 8) + static_cast<int>(header[0]);
    if (size < 0) {
        throw io_exception("Invalid message size: " + string(strerror(errno)));
    }

    // Read the binary protobuf data
    vector<byte> data_buf(size);
    if (ReadBytes(fd, data_buf) != data_buf.size()) {
        throw io_exception("Invalid message data: " + string(strerror(errno)));
    }

    message.ParseFromArray(data_buf.data(), size);
}

size_t protobuf_util::ReadBytes(int fd, span<byte> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = read(fd, &buf.data()[offset], buf.size() - offset);
        if (len == -1) {
            return -1;
        }

        if (!len) {
            break;
        }

        offset += len;
    }

    return offset;
}

size_t protobuf_util::WriteBytes(int fd, span<uint8_t> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = write(fd, &buf.data()[offset], buf.size() - offset);
        if (len == -1) {
            return -1;
        }

        offset += len;

        if (offset == buf.size()) {
            break;
        }
    }

    return offset;
}
