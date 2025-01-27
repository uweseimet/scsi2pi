//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "target_api_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

PbDeviceType target_api_util::ParseDeviceType(const string &value)
{
    if (PbDeviceType type; PbDeviceType_Parse(ToUpper(value), &type)) {
        return type;
    }

    // Handle convenience device types (shortcuts)
    const auto &it = DEVICE_TYPES.find(tolower(value[0]));
    return it != DEVICE_TYPES.end() ? it->second : UNDEFINED;
}

PbCachingMode target_api_util::ParseCachingMode(const string &value)
{
    string v = value;
    ranges::replace(v, '-', '_');

    if (PbCachingMode mode; PbCachingMode_Parse(ToUpper(v), &mode)) {
        return mode;
    }

    throw ParserException("Invalid caching mode '" + value + "'");
}

void target_api_util::ParseParameters(PbDeviceDefinition &device, const string &params)
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

string target_api_util::SetCommandParams(PbCommand &command, const string &params)
{
    if (params.empty()) {
        return "";
    }

    if (params.find(KEY_VALUE_SEPARATOR) != string::npos) {
        return SetFromGenericParams(command, params);
    }

    switch (const auto &components = Split(params, COMPONENT_SEPARATOR, 3); components.size()) {
    case 3:
        SetParam(command, "operations", components[2]);
        [[fallthrough]];

    case 2:
        SetParam(command, "file_pattern", components[1]);
        SetParam(command, "folder_pattern", components[0]);
        break;

    case 1:
        SetParam(command, "file_pattern", components[0]);
        break;

    default:
        assert(false);
        break;
    }

    return "";
}

string target_api_util::SetFromGenericParams(PbCommand &command, const string &params)
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

void target_api_util::SetProductData(PbDeviceDefinition &device, const string &data)
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

string target_api_util::SetIdAndLun(PbDeviceDefinition &device, const string &value)
{
    int id;
    int lun;
    if (const string &error = ParseIdAndLun(value, id, lun); !error.empty()) {
        return error;
    }

    device.set_id(id);
    device.set_unit(lun != -1 ? lun : 0);

    return "";
}

int target_api_util::GetLunMax(PbDeviceType type)
{
    return type == SAHD ? 2 : 32;
}

string target_api_util::ListDevices(const vector<PbDevice> &devices)
{
    if (devices.empty()) {
        return "No devices currently attached\n";
    }

    vector<PbDevice> sorted_devices(devices);
    ranges::sort(sorted_devices, [](const auto &a, const auto &b) {return a.id() < b.id() || a.unit() < b.unit();});

    string s = "+--------+------+-------------------------------------------\n"
        "| ID:LUN | Type | Image File/Device File/Description\n"
        "+--------+------+-------------------------------------------\n";

    for (const auto &device : sorted_devices) {
        s += fmt::format("|  {0}:{1:<2}  | {2} | {3}{4}\n", device.id(), device.unit(),
            PbDeviceType_Name(device.type()), device.file().name(),
            !device.status().removed() && (device.properties().read_only() || device.status().protected_()) ?
                " (READ-ONLY)" : "");
    }

    s += "+--------+------+-------------------------------------------\n";

    return s;
}
