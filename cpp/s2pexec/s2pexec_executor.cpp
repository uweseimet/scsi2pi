//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_executor.h"

string S2pExecExecutor::Init(const string &device)
{
    use_sg = true;

    return sg_adapter->Init(device);
}

int S2pExecExecutor::ExecuteCommand(vector<uint8_t> &cdb, vector<uint8_t> &buf, int timeout, bool log)
{
    if (use_sg) {
        const SgAdapter::SgResult &result = sg_adapter->SendCommand(cdb, buf, static_cast<int>(buf.size()), timeout);
        length = result.length;
        return result.status;
    }

    return initiator_executor->Execute(cdb, buf, buf.size(), timeout, log);
}

tuple<sense_key, asc, int> S2pExecExecutor::GetSenseData() const
{
    if (use_sg) {
        array<uint8_t, 14> sense_data;
        vector<uint8_t> cdb(6);
        cdb[0] = static_cast<uint8_t>(scsi_command::request_sense);
        cdb[4] = static_cast<uint8_t>(sense_data.size());

        sg_adapter->SendCommand(cdb, sense_data, static_cast<int>(sense_data.size()), 1);

        return {static_cast<sense_key>(static_cast<int>(sense_data[0]) & 0x0f), static_cast<asc>(sense_data[12]), sense_data[13]};
    }

    return initiator_util::GetSenseData(*initiator_executor);
}

int S2pExecExecutor::GetByteCount() const
{
    return use_sg ? sg_adapter->GetByteCount() : initiator_executor->GetByteCount();
}

void S2pExecExecutor::SetTarget(int id, int lun, bool sasi)
{
    initiator_executor->SetTarget(id, lun, sasi);
}
