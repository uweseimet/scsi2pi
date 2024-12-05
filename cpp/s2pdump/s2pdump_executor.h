//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <set>
#include "initiator/initiator_executor.h"

using namespace std;

class S2pDumpExecutor
{

public:

    S2pDumpExecutor(Bus &bus, int id, logger &l) : initiator_executor(make_unique<InitiatorExecutor>(bus, id, l))
    {
    }
    ~S2pDumpExecutor() = default;

    void TestUnitReady() const;
    void RequestSense() const;
    bool Inquiry(span<uint8_t>);
    pair<uint64_t, uint32_t> ReadCapacity();
    bool ReadWrite(span<uint8_t>, uint32_t, uint32_t, int, bool);
    bool ModeSense6(span<uint8_t>);
    void SynchronizeCache();
    set<int> ReportLuns();

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

private:

    static uint32_t GetInt32(span<uint8_t>, int = 0);
    static uint64_t GetInt64(span<uint8_t>, int = 0);

    unique_ptr<InitiatorExecutor> initiator_executor;
};
