//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <array>
#include "shared/phase_executor.h"
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class S2pDumpExecutor
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

    S2pDumpExecutor(Bus &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~S2pDumpExecutor() = default;

    string Execute(const string&, protobuf_format, PbResult&);
    bool ShutDown();

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

private:

    array<uint8_t, BUFFER_SIZE> buffer;

    unique_ptr<PhaseExecutor> phase_executor;
};
