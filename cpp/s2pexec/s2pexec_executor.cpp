//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "initiator/initiator_util.h"
#include "s2pexec_executor.h"

using namespace std;
using namespace initiator_util;

int S2pExecExecutor::ExecuteCommand(scsi_command cmd, vector<uint8_t> &cdb, vector<uint8_t> &buffer)
{
    const int status = initiator_executor->Execute(cmd, cdb, buffer, buffer.size());

    LogStatus(status);

    return status;
}
