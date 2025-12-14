//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "cache.h"

class DiskTrack;

class DiskCache : public Cache
{

public:

    DiskCache(const string&, int, uint64_t);
    ~DiskCache() override = default;

    bool Init() override;
    bool Flush() override;
    int ReadSectors(data_in_t, uint64_t, uint32_t) override;
    int WriteSectors(data_out_t, uint64_t, uint32_t) override;

    vector<PbStatistics> GetStatistics(const Device&) const override;

private:

    using CacheData = struct {
        shared_ptr<DiskTrack> disktrk;
        uint32_t serial;
    };

    shared_ptr<DiskTrack> AssignTrack(int64_t);
    shared_ptr<DiskTrack> GetTrack(uint64_t);
    bool Load(int index, int64_t track, shared_ptr<DiskTrack>);
    void UpdateSerial();

    // Number of tracks to cache
    static constexpr int64_t CACHE_MAX = 16;

    array<CacheData, CACHE_MAX> cache = { };

    // Last serial number
    uint32_t serial = 0;

    string sec_path;

    // Sector size shift  (8 = 256, 9 = 512, 10 = 1024, 11 = 2048, 12 = 4096)
    int shift_count = 8;

    int64_t blocks;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
    uint64_t cache_miss_read_count = 0;
    uint64_t cache_miss_write_count = 0;
};

