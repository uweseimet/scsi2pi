//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <set>
#include <span>
#include <spdlog/spdlog.h>
#include "initiator/initiator_executor.h"
#include "s2pdump_executor.h"

using namespace std;

class BoardExecutor : public S2pDumpExecutor
{

public:

    BoardExecutor(Bus &bus, int id, logger &l) : S2pDumpExecutor(l), initiator_executor(
        make_unique<InitiatorExecutor>(bus, id, l))
    {
    }
    virtual ~BoardExecutor() = default;

    // Disk and tape support
    void TestUnitReady(vector<uint8_t>&) const override;
    int RequestSense(vector<uint8_t>&, span<uint8_t>) const override;
    bool Inquiry(vector<uint8_t>&, span<uint8_t>) const override;
    bool ModeSense6(vector<uint8_t>&, span<uint8_t>) const override;
    set<int> ReportLuns(vector<uint8_t>&, span<uint8_t>) override;

    // Disk support
    int ReadCapacity10(vector<uint8_t>&, span<uint8_t>) const override;
    int ReadCapacity16(vector<uint8_t>&, span<uint8_t>) const override;
    bool ReadWrite(vector<uint8_t>&, span<uint8_t>, int) override;
    void SynchronizeCache(vector<uint8_t>&) const override;

    // Tape support
    int Rewind(vector<uint8_t>&) const override;
    int WriteFilemark(vector<uint8_t>&) const override;
    int Read(vector<uint8_t>&, span<uint8_t>, int, int) override;
    int Write(vector<uint8_t>&, span<uint8_t>, int, int) override;

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

protected:

    void SpaceBack(vector<uint8_t>&) const override;

private:

    unique_ptr<InitiatorExecutor> initiator_executor;
};
