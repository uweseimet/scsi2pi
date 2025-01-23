//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <spdlog/spdlog.h>
#include "initiator_executor.h"

using namespace std;
using namespace spdlog;

namespace initiator_util
{

void ResetBus(Bus&);
tuple<SenseKey, Asc, int> GetSenseData(InitiatorExecutor&);
bool SetLogLevel(logger&, const string&);

}
