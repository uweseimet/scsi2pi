//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_executor.h"

int S2pExecExecutor::ExecuteCommand(scsi_command cmd, vector<uint8_t> &cdb, vector<uint8_t> &buffer, int timeout)
{
    return initiator_executor->Execute(cmd, cdb, buffer, buffer.size(), timeout);
}
