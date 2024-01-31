//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
// The DeviceFactory creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include "primary_device.h"
#include "generated/s2p_interface.pb.h"
#include "shared/s2p_util.h"

using namespace std;
using namespace s2p_interface;

class DeviceFactory
{
    using extension_mapping = unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>>;

public:

    static DeviceFactory& Instance();

    shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
    PbDeviceType GetTypeForFile(const string&) const;

    extension_mapping GetExtensionMapping() const
    {
        return mapping;
    }
    bool AddExtensionMapping(const string&, PbDeviceType) const;

private:

    DeviceFactory() = default;

    const inline static unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> DEVICE_MAPPING = {
        { "daynaport", SCDP },
        { "printer", SCLP },
        { "services", SCHS }
    };

    inline static extension_mapping mapping;
};
