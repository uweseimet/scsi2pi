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
#include "s2pdump_executor.h"

class SgAdapter;
namespace spdlog
{
class logger;
}

using namespace std;
using namespace spdlog;

class SgExecutor : public S2pDumpExecutor
{

public:

    SgExecutor(SgAdapter &adapter, logger &l) : S2pDumpExecutor(l), sg_adapter(adapter)
    {
    }
    virtual ~SgExecutor() = default;

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
    bool Read(vector<uint8_t>&, span<uint8_t>, int) override;
    bool Write(vector<uint8_t>&, span<uint8_t>, int) override;

protected:

    void SpaceBack(vector<uint8_t>&) const override;

private:

    SgAdapter &sg_adapter;
};
