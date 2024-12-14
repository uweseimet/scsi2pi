//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "device_logger.h"

using namespace spdlog;

#ifdef NOLOG_TRACE
void DeviceLogger::Trace(const string &) const
{
    // Do nothing
}
#else
void DeviceLogger::Trace(const string &s) const
{
    Log(level::trace, s);
}
#endif

#ifdef NOLOG_DEBUG
void DeviceLogger::Debug(const string &) const
{
    // Do nothing
}
#else
void DeviceLogger::Debug(const string &s) const
{
    Log(level::trace, s);
}
#endif

void DeviceLogger::Info(const string &s) const
{
    Log(level::info, s);
}

void DeviceLogger::Warn(const string &s) const
{
    Log(level::warn, s);
}

void DeviceLogger::Error(const string &s) const
{
    Log(level::err, s);
}

void DeviceLogger::Log(level::level_enum level, const string &s) const
{
    if ((log_device_id == -1 || log_device_id == id) && (lun == -1 || log_device_lun == -1 || log_device_lun == lun)) {
        if (lun == -1) {
            log(level, "(ID {0}) - {1}", id, s);
        }
        else {
            log(level, "(ID:LUN {0}:{1}) - {2}", id, lun, s);
        }
    }
}

void DeviceLogger::SetIdAndLun(int i, int l)
{
    id = i;
    lun = l;
}

void DeviceLogger::SetLogIdAndLun(int i, int l)
{
    log_device_id = i;
    log_device_lun = l;
}
