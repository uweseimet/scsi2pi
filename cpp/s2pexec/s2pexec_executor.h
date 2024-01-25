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
#include "shared_initiator/phase_executor.h"

using namespace std;

class S2pExecExecutor
{

    // The SCSI ExecuteOperation command supports a byte count of up to 65535 bytes
    inline static const int BUFFER_SIZE = 65535;

public:

    enum class protobuf_format
    {
        binary = 0b001,
        json = 0b010,
        text = 0b100
    };

    S2pExecExecutor(Bus &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~S2pExecExecutor() = default;

    bool ExecuteCommand(scsi_command, vector<uint8_t>&, vector<uint8_t>&, bool);
    string GetSenseData(bool);

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

    int GetByteCount() const
    {
        return phase_executor->GetByteCount();
    }

private:

    unique_ptr<PhaseExecutor> phase_executor;
};
