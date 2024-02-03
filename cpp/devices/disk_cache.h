//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

#include <span>
#include <array>
#include <memory>
#include "disk_track.h"
#include "cache.h"

class DiskCache : public Cache
{
    // Number of tracks to cache
    static const int CACHE_MAX = 16;

public:

    using cache_t = struct {
        shared_ptr<DiskTrack> disktrk;
        uint32_t serial;
    };

    DiskCache(const string&, int, uint32_t, bool);
    ~DiskCache() override = default;

    bool Flush() override;
    bool ReadSector(span<uint8_t>, uint32_t) override;
    bool WriteSector(span<const uint8_t>, uint32_t) override;

    vector<PbStatistics> GetStatistics(bool) const override;

private:

    shared_ptr<DiskTrack> Assign(int);
    shared_ptr<DiskTrack> GetTrack(uint32_t);
    bool Load(int index, int track, shared_ptr<DiskTrack>);
    void UpdateSerialNumber();

    // Internal datay
    array<cache_t, CACHE_MAX> cache = { }; // Cache management
    uint32_t serial = 0; // Last serial number
    string sec_path; // Path
    int sec_size; // Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
    int sec_blocks; // Blocks per sector

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
    uint64_t cache_miss_read_count = 0;
    uint64_t cache_miss_write_count = 0;

    static inline const unordered_map<uint32_t, uint32_t> SHIFT_COUNTS =
        { { 256, 8 }, { 512, 9 }, { 1024, 10 }, { 2048, 11 }, { 4096, 12 } };
};

