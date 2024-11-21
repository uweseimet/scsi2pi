//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <spdlog/spdlog.h>

using namespace std;

class DeviceLogger
{

public:

    void Trace(const string&) const;
    void Debug(const string&) const;
    void Info(const string&) const;
    void Warn(const string&) const;
    void Error(const string&) const;

    void SetIdAndLun(int, int);
    static void SetLogIdAndLun(int, int);

private:

    void Log(spdlog::level::level_enum, const string&) const;

    int id = -1;
    int lun = -1;

    static inline int log_device_id = -1;
    static inline int log_device_lun = -1;
};
