//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "device.h"
#include <stdexcept>

void Device::SetProtected(bool b)
{
    if (protectable && !read_only) {
        write_protected = b;
    }
}

string Device::GetParam(const string &key) const
{
    const auto &it = params.find(key);
    return it == params.end() ? "" : it->second;
}

void Device::SetParams(const param_map &set_params)
{
    params = GetDefaultParams();

    // Devices with image file support implicitly support the "file" parameter
    if (SupportsImageFile()) {
        params["file"].clear();
    }

    for (const auto& [key, value] : set_params) {
        // It is assumed that there are defaults for all supported parameters
        if (params.contains(key)) {
            params[key] = value;
        }
        else {
            spdlog::warn("{0} ignored unknown parameter '{1}={2}'", PbDeviceType_Name(type), key, value);
        }
    }
}

bool Device::Start()
{
    if (!ready) {
        return false;
    }

    stopped = false;

    return true;
}

void Device::Stop()
{
    ready = false;
    attn = false;
    stopped = true;
}

bool Device::Eject(bool force)
{
    if (!ready || !removable) {
        return false;
    }

    // Must be unlocked if there is no force flag
    if (!force && locked) {
        return false;
    }

    ready = false;
    attn = false;
    removed = true;
    write_protected = false;
    locked = false;
    stopped = true;

    return true;
}
