//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
// The DeviceFactory creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include "primary_device.h"
#include "generated/s2p_interface.pb.h"
#include "shared/s2p_util.h"

using namespace std;
using namespace s2p_interface;

class DeviceFactory
{

public:

    DeviceFactory() = default;
    ~DeviceFactory() = default;

    shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
    PbDeviceType GetTypeForFile(const string&) const;
    unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> GetExtensionMapping() const;

private:

    const inline static unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> DEVICE_MAPPING = {
        { "daynaport", SCDP },
        { "printer", SCLP },
        { "services", SCHS }
    };
};
