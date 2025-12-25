//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/s2p_defs.h"
#include "generated/s2p_interface.pb.h"

using namespace s2p_interface;

class Device;

class Cache
{

public:

    virtual ~Cache() = default;

    virtual int ReadSectors(data_in_t, uint64_t, uint32_t) = 0;
    virtual int WriteSectors(data_out_t, uint64_t, uint32_t) = 0;

    virtual bool Flush() = 0;

    virtual bool Init() = 0;

    virtual vector<PbStatistics> GetStatistics(const Device&) const = 0;

protected:

    Cache() = default;

    static constexpr const char *READ_ERROR_COUNT = "read_error_count";
    static constexpr const char *WRITE_ERROR_COUNT = "write_error_count";
    static constexpr const char *CACHE_MISS_READ_COUNT = "cache_miss_read_count";
    static constexpr const char *CACHE_MISS_WRITE_COUNT = "cache_miss_write_count";
};
