//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/s2p_util.h"
#include "s2pexec_executor.h"

using namespace std;
using namespace scsi_defs;
using namespace s2p_util;

bool S2pExecExecutor::ExecuteCommand(scsi_command cmd, vector<uint8_t> &cdb, vector<uint8_t> &buffer, bool sasi)
{
    return phase_executor->Execute(cmd, cdb, buffer, buffer.size(), sasi);
}

string S2pExecExecutor::GetSenseData(bool sasi)
{
    vector<uint8_t> buf(14);
    array<uint8_t, 6> cdb = { };
    cdb[4] = buf.size();

    if (!phase_executor->Execute(scsi_command::cmd_request_sense, cdb, buf, buf.size(), sasi)) {
        return "Can't execute REQUEST SENSE";
    }

    if (phase_executor->GetByteCount() < static_cast<int>(buf.size())) {
        return "Device reported an unknown error";
    }
    else {
        return FormatSenseData(static_cast<sense_key>(buf[2] & 0x0f), static_cast<asc>(buf[12]), buf[13]);
    }
}
