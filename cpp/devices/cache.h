//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class Cache
{

public:

    virtual ~Cache() = default;

    virtual int ReadSectors(span<uint8_t>, uint64_t, uint32_t) = 0;
    virtual int WriteSectors(span<const uint8_t>, uint64_t, uint32_t) = 0;

    virtual bool Flush() = 0;

    virtual bool Init() = 0;

    virtual vector<PbStatistics> GetStatistics(bool) const = 0;

protected:

    Cache() = default;

    static constexpr const char *READ_ERROR_COUNT = "read_error_count";
    static constexpr const char *WRITE_ERROR_COUNT = "write_error_count";
    static constexpr const char *CACHE_MISS_READ_COUNT = "cache_miss_read_count";
    static constexpr const char *CACHE_MISS_WRITE_COUNT = "cache_miss_write_count";
};
