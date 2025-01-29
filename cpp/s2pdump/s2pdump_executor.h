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
#include <vector>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

class S2pDumpExecutor
{

public:

    // Disk and tape support
    void TestUnitReady() const;
    void RequestSense(span<uint8_t>) const;
    bool Inquiry(span<uint8_t>) const;
    bool ModeSense6(span<uint8_t>) const;
    set<int> ReportLuns();

    // Disk support
    pair<uint64_t, uint32_t> ReadCapacity() const;
    bool ReadWrite(span<uint8_t>, uint32_t, uint32_t, int, bool);
    void SynchronizeCache() const;

    // Tape support
    int Rewind();
    int WriteFilemark() const;
    int ReadWrite(span<uint8_t>, int);

    logger &s2pdump_logger;

    static const int NO_MORE_DATA = -1;
    static const int BAD_BLOCK = -2;

    static const int SHORT_TIMEOUT = 3;
    static const int LONG_TIMEOUT = 300;

protected:

    explicit S2pDumpExecutor(logger &l) : s2pdump_logger(l)
    {
    }
    virtual ~S2pDumpExecutor() = default;

    // Disk and tape support
    virtual void TestUnitReady(span<uint8_t>) const = 0;
    virtual int RequestSense(span<uint8_t>, span<uint8_t>) const = 0;
    virtual bool Inquiry(span<uint8_t>, span<uint8_t>) const = 0;
    virtual bool ModeSense6(span<uint8_t>, span<uint8_t>) const = 0;
    virtual set<int> ReportLuns(span<uint8_t>, span<uint8_t>) = 0;

    // Disk support
    virtual int ReadCapacity10(span<uint8_t>, span<uint8_t>) const = 0;
    virtual int ReadCapacity16(span<uint8_t>, span<uint8_t>) const = 0;
    virtual bool ReadWrite(span<uint8_t>, span<uint8_t>, int) = 0;
    virtual void SynchronizeCache(span<uint8_t>) const = 0;

    // Tape support
    virtual int Rewind(span<uint8_t>) const = 0;
    virtual int WriteFilemark(span<uint8_t>) const = 0;
    virtual bool Read(span<uint8_t>, span<uint8_t>, int) = 0;
    virtual bool Write(span<uint8_t>, span<uint8_t>, int) = 0;

    void SpaceBack() const;
    virtual void SpaceBack(span<uint8_t>) const = 0;

    static void SetInt24(span<uint8_t>, int, int);

    logger& GetLogger() const
    {
        return s2pdump_logger;
    }

private:

    int default_length = 0xffffff;
};
