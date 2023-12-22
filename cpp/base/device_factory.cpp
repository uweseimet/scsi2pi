//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#if defined BUILD_SCHD || defined BUILD_SCRM
#include "devices/scsi_hd.h"
#endif
#ifdef BUILD_SCMO
#include "devices/optical_memory.h"
#endif
#ifdef BUILD_SCCD
#include "devices/scsi_cd.h"
#endif
#ifdef BUILD_SCDP
#include "devices/daynaport.h"
#endif
#ifdef BUILD_SCLP
#include "devices/printer.h"
#endif
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#ifdef BUILD_SAHD
#include "devices/sasi_hd.h"
#endif
#include "device_factory.h"

using namespace std;
using namespace s2p_util;

shared_ptr<PrimaryDevice> DeviceFactory::CreateDevice(PbDeviceType type, int lun, const string &filename) const
{
    // If no type was specified try to derive the device type from the filename
    if (type == UNDEFINED) {
        type = GetTypeForFile(filename);
        if (type == UNDEFINED) {
            return nullptr;
        }
    }

    shared_ptr<PrimaryDevice> device;
    switch (type) {

#if defined BUILD_SCHD || defined BUILD_SCRM
    case SCHD: {
        const string ext = GetExtensionLowerCase(filename);
        device = make_shared<ScsiHd>(lun, false, ext == "hda", ext == "hd1");
        break;
    }

    case SCRM:
        device = make_shared<ScsiHd>(lun, true, false, false);
        break;
#endif

#ifdef BUILD_SCMO
    case SCMO:
        device = make_shared<OpticalMemory>(lun);
        break;
#endif

#ifdef BUILD_SCCD
    case SCCD: {
        const string ext = GetExtensionLowerCase(filename);
        device = make_shared<ScsiCd>(lun, ext == "is1");
        break;
    }
#endif

#ifdef BUILD_SCDP
    case SCDP:
        device = make_shared<DaynaPort>(lun);
        break;
#endif

#ifdef BUILD_SCHS
    case SCHS:
        device = make_shared<HostServices>(lun);
        break;
#endif

#ifdef BUILD_SCLP
    case SCLP:
        device = make_shared<Printer>(lun);
        break;
#endif

#ifdef BUILD_SAHD
    case SAHD:
        device = make_shared<SasiHd>(lun);
        break;
#endif

    default:
        break;
    }

    return device;
}

PbDeviceType DeviceFactory::GetTypeForFile(const string &filename) const
{
    const auto &mapping = GetExtensionMapping();
    if (const auto &it = mapping.find(GetExtensionLowerCase(filename)); it != mapping.end()) {
        return it->second;
    }

    if (const auto &it = DEVICE_MAPPING.find(filename); it != DEVICE_MAPPING.end()) {
        return it->second;
    }

    return UNDEFINED;
}

unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> DeviceFactory::GetExtensionMapping() const
{
    unordered_map<string, PbDeviceType, s2p_util::StringHash, equal_to<>> mapping;

#if defined BUILD_SCHD | defined BUILD_SCRM
    mapping["hd1"] = SCHD;
    mapping["hds"] = SCHD;
    mapping["hda"] = SCHD;
    mapping["hdr"] = SCRM;
#endif
#ifdef BUILD_SCMO
    mapping["mos"] = SCMO;
#endif
#ifdef BUILD_SCCD
    mapping["is1"] = SCCD;
    mapping["iso"] = SCCD;
    mapping["cdr"] = SCCD;
    mapping["toast"] = SCCD;
#endif

    return mapping;
}
