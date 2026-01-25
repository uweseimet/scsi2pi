//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "initiator/initiator_executor.h"

namespace s2p_interface
{
class PbResult;
}

using namespace s2p_interface;

class S2pProtoExecutor final
{

public:

    S2pProtoExecutor(Bus &bus, int id, logger &l) : initiator_executor(make_unique<InitiatorExecutor>(bus, id, l))
    {
    }
    ~S2pProtoExecutor() = default;

    string Execute(const string&, ProtobufFormat, PbResult&);

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

    int GetByteCount() const
    {
        return initiator_executor->GetByteCount();
    }

private:

    // The SCSI ExecuteOperation command supports a byte count of up to 65535 bytes
    static constexpr int BUFFER_SIZE = 65535;

    vector<uint8_t> buffer;

    unique_ptr<InitiatorExecutor> initiator_executor;
};
