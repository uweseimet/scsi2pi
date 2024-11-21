//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
// Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2024 Uwe Seimet
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
    int ReadSectors(span<uint8_t>, uint64_t, uint32_t) override;
    int WriteSectors(span<const uint8_t>, uint64_t, uint32_t) override;

    vector<PbStatistics> GetStatistics(bool) const override;

private:

    using cache_t = struct {
        shared_ptr<DiskTrack> disktrk;
        uint32_t serial;
    };

    shared_ptr<DiskTrack> Assign(int);
    shared_ptr<DiskTrack> GetTrack(uint32_t);
    bool Load(int index, int track, shared_ptr<DiskTrack>);
    void UpdateSerialNumber();

    // Number of tracks to cache
    static constexpr int CACHE_MAX = 16;

    array<cache_t, CACHE_MAX> cache = { };
    // Last serial number
    uint32_t serial = 0;
    // Path
    string sec_path;
    // Sector size shift (8=256, 9=512, 10=1024, 11=2048, 12=4096)
    int sec_size = 8;
    // Blocks
    int sec_blocks;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
    uint64_t cache_miss_read_count = 0;
    uint64_t cache_miss_write_count = 0;
};

