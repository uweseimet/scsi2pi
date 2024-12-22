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
#include <vector>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

class S2pDumpExecutor
{

public:

    // Disk and tape support
    void TestUnitReady() const;
    virtual void TestUnitReady(vector<uint8_t>&) const = 0;
    void RequestSense() const;
    virtual int RequestSense(vector<uint8_t>&, span<uint8_t>) const = 0;
    bool Inquiry(span<uint8_t>) const;
    virtual bool Inquiry(vector<uint8_t>&, span<uint8_t>) const = 0;
    bool ModeSense6(span<uint8_t>) const;
    virtual bool ModeSense6(vector<uint8_t>&, span<uint8_t>) const = 0;
    set<int> ReportLuns();
    virtual set<int> ReportLuns(vector<uint8_t>&, span<uint8_t>) = 0;

    // Disk support
    pair<uint64_t, uint32_t> ReadCapacity() const;
    virtual int ReadCapacity10(vector<uint8_t>&, span<uint8_t>) const = 0;
    virtual int ReadCapacity16(vector<uint8_t>&, span<uint8_t>) const = 0;
    bool ReadWrite(span<uint8_t>, uint32_t, uint32_t, int, bool);
    virtual bool ReadWrite(vector<uint8_t>&, span<uint8_t>, int) = 0;
    void SynchronizeCache() const;
    virtual void SynchronizeCache(vector<uint8_t>&) const = 0;

    // Tape support
    int Rewind() const;
    virtual int Rewind(vector<uint8_t>&) const = 0;
    int WriteFilemark() const;
    virtual int WriteFilemark(vector<uint8_t>&) const = 0;
    int ReadWrite(span<uint8_t>, int);
    virtual int Read(vector<uint8_t>&, span<uint8_t>, int, int) = 0;
    virtual int Write(vector<uint8_t>&, span<uint8_t>, int, int) = 0;

    logger &s2pdump_logger;

    static const int NO_MORE_DATA = -1;
    static const int BAD_BLOCK = -2;

    static const int SHORT_TIMEOUT = 3;
    static const int LONG_TIMEOUT = 300;

protected:

    S2pDumpExecutor(logger &l) : s2pdump_logger(l)
    {
    }

    void SpaceBack() const;
    virtual void SpaceBack(vector<uint8_t>&) const = 0;

    static void SetInt24(span<uint8_t>, int, int);

    logger& GetLogger()
    {
        return s2pdump_logger;
    }

    int default_length = 0xffffff;
};
