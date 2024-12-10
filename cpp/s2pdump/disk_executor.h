//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "s2pdump_executor.h"

using namespace std;

class DiskExecutor : public S2pDumpExecutor
{

public:

    DiskExecutor(Bus &b, int i, logger &l) : S2pDumpExecutor(b, i, l)
    {
    }

    pair<uint64_t, uint32_t> ReadCapacity();
    bool ReadWriteDisk(span<uint8_t>, uint32_t, uint32_t, int, bool);
    void SynchronizeCache();

private:

    unique_ptr<S2pDumpExecutor> s2pdump_executor;
};
