//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

namespace logger_util
{

shared_ptr<logger> CreateLogger(const string&);

}
