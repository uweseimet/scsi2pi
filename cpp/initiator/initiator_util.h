//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

namespace initiator_util
{

bool SetLogLevel(logger&, const string&);

}
