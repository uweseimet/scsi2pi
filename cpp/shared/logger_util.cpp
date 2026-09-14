//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "logger_util.h"
#include <spdlog/sinks/stdout_color_sinks.h>

shared_ptr<logger> logger_util::CreateLogger(const string &name)
{
    auto l = spdlog::get(name);
    return l ? l : stdout_color_st(name);
}
