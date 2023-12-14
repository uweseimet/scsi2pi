//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <set>
#include <span>
#include "shared/phase_executor.h"

using namespace std;

class S2pDumpExecutor
{

public:

    S2pDumpExecutor(Bus &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~S2pDumpExecutor() = default;

    void TestUnitReady() const;
    bool Inquiry(span<uint8_t>);
    pair<uint64_t, uint32_t> ReadCapacity();
    bool ReadWrite(span<uint8_t>, uint32_t, uint32_t, int, bool);
    void SynchronizeCache();
    set<int> ReportLuns();

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

private:

    static uint32_t GetInt32(span<uint8_t>, int = 0);
    static uint64_t GetInt64(span<uint8_t>, int = 0);

    unique_ptr<PhaseExecutor> phase_executor;
};
