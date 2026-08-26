//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
// The DeviceFactory creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include "shared/s2p_util.h"
#include "generated/s2p_interface.pb.h"

using namespace s2p_interface;

class PrimaryDevice;

class DeviceFactory final
{

public:

    DeviceFactory(const DeviceFactory&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = delete;

    static DeviceFactory& GetInstance()
    {
        static DeviceFactory instance; // NOSONAR Singleton with mutable internal state
        return instance;
    }

    shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
    PbDeviceType GetTypeForFile(const string&) const;

    const auto& GetExtensionMapping() const
    {
        return mapping;
    }
    bool AddExtensionMapping(const string&, PbDeviceType);

private:

    DeviceFactory();

    inline static const unordered_map<string_view, PbDeviceType> DEVICE_MAPPING = {
        { "daynaport", SCDP },
        { "printer", SCLP },
        { "services", SCHS }
    };

    unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> mapping;
};
