//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "device_factory.h"
#include <filesystem>
#ifdef BUILD_SCDP
#include "devices/daynaport.h"
#endif
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#ifdef BUILD_SCMO
#include "devices/optical_memory.h"
#endif
#ifdef BUILD_SCLP
#include "devices/printer.h"
#endif
#ifdef BUILD_SAHD
#include "devices/sasi_hd.h"
#endif
#ifdef BUILD_SCCD
#include "devices/scsi_cd.h"
#endif
#ifdef BUILD_SCTP
#include "devices/tape.h"
#endif
#if defined BUILD_SCHD || defined BUILD_SCRM
#include "devices/scsi_hd.h"
#endif
#include "shared/s2p_util.h"

using namespace s2p_util;

DeviceFactory::DeviceFactory()
{
#if defined BUILD_SCHD || defined BUILD_SCRM
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
#ifdef BUILD_SCTP
    mapping["tar"] = SCTP;
    mapping["tap"] = SCTP;
#endif
}

shared_ptr<PrimaryDevice> DeviceFactory::CreateDevice(PbDeviceType type, int lun, const string &filename) const
{
    // If no type was specified try to derive the device type from the filename
    if (type == UNDEFINED) {
        type = GetTypeForFile(filename);
        if (type == UNDEFINED) {
            return nullptr;
        }
    }

    switch (type) {

#if defined BUILD_SCHD || defined BUILD_SCRM
    case SCHD: {
        const string &ext = GetExtensionLowerCase(filename);
        return make_shared<ScsiHd>(lun, false, ext == "hda", ext == "hd1");
    }

    case SCRM:
        return make_shared<ScsiHd>(lun, true, false, false);
#endif

#ifdef BUILD_SCMO
    case SCMO:
        return make_shared<OpticalMemory>(lun);
#endif

#ifdef BUILD_SCCD
    case SCCD: {
        const string &ext = GetExtensionLowerCase(filename);
        return make_shared<ScsiCd>(lun, ext == "is1");
    }
#endif

#ifdef BUILD_SCTP
    case SCTP:
        return make_shared<Tape>(lun);
#endif

#ifdef BUILD_SCDP
    case SCDP:
        return make_shared<DaynaPort>(lun);
#endif

#ifdef BUILD_SCHS
    case SCHS:
        return make_shared<HostServices>(lun);
#endif

#ifdef BUILD_SCLP
    case SCLP:
        return make_shared<Printer>(lun);
#endif

#ifdef BUILD_SAHD
    case SAHD:
        return make_shared<SasiHd>(lun);
#endif

    default:
        break;
    }

    return nullptr;
}

PbDeviceType DeviceFactory::GetTypeForFile(const string &filename) const
{
    if (const auto &it = mapping.find(GetExtensionLowerCase(filename)); it != mapping.end()) {
        return it->second;
    }

    if (const auto &it = DEVICE_MAPPING.find(filename); it != DEVICE_MAPPING.end()) {
        return it->second;
    }

    return UNDEFINED;
}

bool DeviceFactory::AddExtensionMapping(const string &extension, PbDeviceType type)
{
    if (mapping.contains(extension)) {
        return false;
    }

    mapping[extension] = type;

    return true;
}
