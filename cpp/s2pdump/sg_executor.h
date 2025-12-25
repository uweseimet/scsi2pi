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
#include "s2pdump_executor.h"

class SgAdapter;

class SgExecutor final : public S2pDumpExecutor
{

public:

    SgExecutor(SgAdapter &adapter, logger &l) : S2pDumpExecutor(l), sg_adapter(adapter)
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

protected:

    void SpaceBack(span<uint8_t>) const override;

private:

    SgAdapter &sg_adapter;
};
