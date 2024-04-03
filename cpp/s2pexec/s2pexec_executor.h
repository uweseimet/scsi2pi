//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "initiator/initiator_util.h"

using namespace std;

class S2pExecExecutor
{

public:

    enum class protobuf_format
    {
        binary = 0b001,
        json = 0b010,
        text = 0b100
    };

    S2pExecExecutor(Bus &bus, int id) : initiator_executor(make_unique<InitiatorExecutor>(bus, id))
    {
    }
    ~S2pExecExecutor() = default;

    int ExecuteCommand(scsi_command, vector<uint8_t>&, vector<uint8_t>&, int);

    tuple<sense_key, asc, int> GetSenseData() const
    {
        return initiator_util::GetSenseData(*initiator_executor);
    }

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

    int GetByteCount() const
    {
        return initiator_executor->GetByteCount();
    }

private:

    unique_ptr<InitiatorExecutor> initiator_executor;

    // The SCSI ExecuteOperation custom command supports a byte count of up to 65535 bytes
    static constexpr int BUFFER_SIZE = 65535;
};
