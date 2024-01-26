//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <array>
#include "initiator/initiator_util.h"
#include "initiator/initiator_executor.h"

using namespace std;

class S2pExecExecutor
{
    // The SCSI ExecuteOperation custom command supports a byte count of up to 65535 bytes
    inline static const int BUFFER_SIZE = 65535;

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

    void Sasi(bool sasi)
    {
        initiator_executor->Sasi(sasi);
    }

    int ExecuteCommand(scsi_command, vector<uint8_t>&, vector<uint8_t>&);

    string GetSenseData() const
    {
        return initiator_util::GetSenseData(*initiator_executor);
    }

    void SetTarget(int id, int lun)
    {
        initiator_executor->SetTarget(id, lun);
    }

    int GetByteCount() const
    {
        return initiator_executor->GetByteCount();
    }

private:

    unique_ptr<InitiatorExecutor> initiator_executor;
};
