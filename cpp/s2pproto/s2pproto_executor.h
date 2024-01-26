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

using namespace std;
using namespace s2p_interface;

class S2pProtoExecutor
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

    S2pProtoExecutor(Bus &bus, int id) : initiator_executor(make_unique<InitiatorExecutor>(bus, id))
    {
    }
    ~S2pProtoExecutor() = default;

    string Execute(const string&, protobuf_format, PbResult&);

    void SetTarget(int id, int lun)
    {
        initiator_executor->SetTarget(id, lun);
    }

    int GetByteCount() const
    {
        return initiator_executor->GetByteCount();
    }

private:

    array<uint8_t, BUFFER_SIZE> buffer;

    unique_ptr<InitiatorExecutor> initiator_executor;
};
