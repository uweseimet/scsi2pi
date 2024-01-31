//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "initiator_executor.h"

using namespace std;

namespace initiator_util
{
tuple<sense_key, asc, int> GetSenseData(InitiatorExecutor&);
bool SetLogLevel(const string&);
}
