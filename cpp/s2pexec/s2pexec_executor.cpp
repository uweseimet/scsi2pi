//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_executor.h"

int S2pExecExecutor::ExecuteCommand(vector<uint8_t> &cdb, vector<uint8_t> &buffer, int timeout, bool log)
{
    return initiator_executor->Execute(cdb, buffer, buffer.size(), timeout, log);
}
