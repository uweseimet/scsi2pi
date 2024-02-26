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
#include "initiator/initiator_executor.h"
#include "generated/s2p_interface.pb.h"

using namespace s2p_interface;

class S2pProtoExecutor
{

public:

    enum class protobuf_format
    {
        binary = 0b001,
        json = 0b010,
        text = 0b100
    };

    S2pProtoExecutor(Bus &bus, int id) : initiator_executor(make_unique<InitiatorExecutor>(bus, id))
    {
    }
    ~S2pProtoExecutor() = default;

    string Execute(const string&, protobuf_format, PbResult&);

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

    array<uint8_t, BUFFER_SIZE> buffer;

    unique_ptr<InitiatorExecutor> initiator_executor;
};
