//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <set>
#include <span>
#include <spdlog/spdlog.h>
#include "initiator/initiator_executor.h"
#include "s2pdump_executor.h"

class BoardExecutor final : public S2pDumpExecutor
{

public:

    BoardExecutor(Bus &bus, int id, logger &l) : S2pDumpExecutor(l), initiator_executor(
        make_unique<InitiatorExecutor>(bus, id, l))
    {
    }

    // Disk and tape support
    void TestUnitReady(span<uint8_t>) const override;
    int RequestSense(span<uint8_t>, span<uint8_t>) const override;
    bool Inquiry(span<uint8_t>, span<uint8_t>) const override;
    bool ModeSense6(span<uint8_t>, span<uint8_t>) const override;
    set<int> ReportLuns(span<uint8_t>, span<uint8_t>) override;

    // Disk support
    int ReadCapacity10(span<uint8_t>, span<uint8_t>) const override;
    int ReadCapacity16(span<uint8_t>, span<uint8_t>) const override;
    bool ReadWrite(span<uint8_t>, span<uint8_t>, int) override;
    void SynchronizeCache(span<uint8_t>) const override;

    // Tape support
    int Rewind(span<uint8_t>) const override;
    int WriteFilemark(span<uint8_t>) const override;
    bool Read(span<uint8_t>, span<uint8_t>, int) override;
    bool Write(span<uint8_t>, span<uint8_t>, int) override;

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

protected:

    void SpaceBack(span<uint8_t>) const override;

private:

    bool Execute(ScsiCommand, span<uint8_t>, span<uint8_t>, int, int, bool) const;

    unique_ptr<InitiatorExecutor> initiator_executor;
};
