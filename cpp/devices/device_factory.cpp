//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "device_factory.h"
#include <filesystem>
#ifdef BUILD_SCDP
#include "daynaport.h"
#endif
#ifdef BUILD_SCHS
#include "host_services.h"
#endif
#ifdef BUILD_SCMO
#include "optical_memory.h"
#endif
#ifdef BUILD_SCLP
#include "printer.h"
#endif
#ifdef BUILD_SAHD
#include "sasi_hd.h"
#endif
#ifdef BUILD_SCCD
#include "scsi_cd.h"
#endif
#ifdef BUILD_SCTP
#include "tape.h"
#endif
#if defined BUILD_SCHD
#include "scsi_hd.h"
#endif
#ifdef BUILD_SCSG
#include "scsi_generic.h"
#endif

using namespace s2p_util;

DeviceFactory::DeviceFactory()
{
#ifdef BUILD_SAHD
    mapping["hdf"] = SAHD;
#endif
#ifdef BUILD_SCHD
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
    }

    switch (type) {
#if defined BUILD_SCHD
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
    case SCCD:
        return make_shared<ScsiCd>(lun, GetExtensionLowerCase(filename) == "is1");
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

#ifdef BUILD_SCSG
    case SCSG:
        return make_shared<ScsiGeneric>(lun, filename);
#endif

#ifdef BUILD_SAHD
    case SAHD:
        return make_shared<SasiHd>(lun);
#endif

    default:
        return nullptr;
    }
}

PbDeviceType DeviceFactory::GetTypeForFile(const string &filename) const
{
    if (const auto &it = mapping.find(GetExtensionLowerCase(filename)); it != mapping.end()) {
        return it->second;
    }

    if (const auto &it = ALIAS_MAPPING.find(filename); it != ALIAS_MAPPING.end()) {
        return it->second;
    }

    if (filename.starts_with("/dev/sd")) {
        return SCHD;
    }

    if (filename.starts_with("/dev/sg")) {
        return SCSG;
    }

    if (filename.starts_with("/dev/sr")) {
        return SCCD;
    }

    return UNDEFINED;
}

bool DeviceFactory::AddExtensionMapping(const string &ext, PbDeviceType type)
{
    string extension = ToLower(ext);
    if (extension.starts_with('.')) {
        extension.erase(0, 1);
    }

    if (mapping.contains(extension)) {
        return false;
    }

    mapping[extension] = type;

    return true;
}
