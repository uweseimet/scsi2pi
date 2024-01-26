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
string GetSenseData(InitiatorExecutor&);
void LogStatus(int);
bool SetLogLevel(const string&);
}
