//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <tuple>
#include "shared/scsi.h"

class Bus;
class InitiatorExecutor;
namespace spdlog
{
class logger;
}

using namespace std;
using namespace spdlog;

namespace initiator_util
{

void ResetBus(Bus&);
tuple<SenseKey, Asc, int> GetSenseData(InitiatorExecutor&);
bool SetLogLevel(logger&, const string&);

}
