//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
//   Copyright (C) 2010-2015 isaki@NetBSD.org
//   Copyright (C) 2010 Y.Sugahara
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "disk_cache.h"
#include <cassert>
#include "base/device.h"
#include "disk_track.h"

DiskCache::DiskCache(const string &path, int size, uint64_t sectors) : sec_path(path), blocks(sectors)
{
    while ((1 << shift_count) != size) {
        ++shift_count;
    }
    assert(shift_count >= 8 && shift_count <= 12);
}

bool DiskCache::Init()
{
    return blocks && !sec_path.empty();
}

bool DiskCache::Flush()
{
    // Save valid tracks
    return ranges::none_of(cache, [this](const CacheData &c)
        {   return c.disktrk && !c.disktrk->Save(sec_path, cache_miss_write_count);});
}

shared_ptr<DiskTrack> DiskCache::GetTrack(uint64_t sector)
{
    // Update first
    UpdateSerial();

    // Calculate track (fixed to 256 sectors/track)
    const int64_t track = sector >> 8;

    // Get track data
    return AssignTrack(track);
}

int DiskCache::ReadSectors(data_in_t buf, uint64_t sector, uint32_t count)
{
    assert(count == 1);
    if (count != 1) {
        return 0;
    }

    auto disktrk = GetTrack(sector);
    if (!disktrk) {
        return 0;
    }

    // Read the track data to the cache
    return disktrk->ReadSector(buf, sector & 0xff);
}

int DiskCache::WriteSectors(data_out_t buf, uint64_t sector, uint32_t count)
{
    assert(count == 1);
    if (count != 1) {
        return 0;
    }

    auto disktrk = GetTrack(sector);
    if (!disktrk) {
        return 0;
    }

    // Write the data to the cache
    return disktrk->WriteSector(buf, sector & 0xff);
}

shared_ptr<DiskTrack> DiskCache::AssignTrack(int64_t track)
{
    // Check if it is already assigned
    for (CacheData &c : cache) {
        if (c.disktrk && c.disktrk->GetTrack() == track) {
            // Track match
            c.serial = serial;
            return c.disktrk;
        }
    }

    // Check for an empty cache slot
    for (CacheData &c : cache) {
        if (!c.disktrk && Load(static_cast<int>(&c - &cache[0]), track, nullptr)) {
            c.serial = serial;
            return c.disktrk;
        }
    }

    // Find the cache entry with the smallest serial number, i.e. the oldest entry and save this track, then
    // load/overwrite this track
    if (auto c = ranges::min_element(cache,
        [](const CacheData &d1, const CacheData &d2) {return d1.serial < d2.serial;})
        - cache.begin(); cache[c].disktrk->Save(sec_path, cache_miss_write_count) &&
        Load(static_cast<int>(c), track, std::move(cache[c].disktrk))) {
        cache[c].serial = serial;
        return cache[c].disktrk;
    }

    // Save or load failed
    return nullptr;
}

bool DiskCache::Load(int index, int64_t track, shared_ptr<DiskTrack> disktrk)
{
    assert(index >= 0 && index < static_cast<int>(cache.size()));
    assert(track >= 0);
    assert(!cache[index].disktrk);

    // Get the number of sectors on this track
    int64_t sectors = blocks - (track << 8);
    assert(sectors > 0);
    if (sectors > 0x100) {
        sectors = 0x100;
    }

    if (!disktrk) {
        disktrk = make_shared<DiskTrack>();
    }

    disktrk->Init(track, shift_count, static_cast<int>(sectors));

    // Try loading
    if (!disktrk->Load(sec_path, cache_miss_read_count)) {
        ++read_error_count;

        return false;
    }

    // Allocation successful, work set
    cache[index].disktrk = disktrk;

    return true;
}

void DiskCache::UpdateSerial()
{
    // Update and do nothing except 0
    ++serial;
    if (serial != 0) {
        return;
    }

    // Clear serial of all caches
    for (CacheData &c : cache) {
        c.serial = 0;
    }
}

vector<PbStatistics> DiskCache::GetStatistics(const Device &device) const
{
    vector<PbStatistics> statistics;

    device.EnrichStatistics(statistics, CATEGORY_INFO, CACHE_MISS_READ_COUNT, cache_miss_read_count);
    device.EnrichStatistics(statistics, CATEGORY_ERROR, READ_ERROR_COUNT, read_error_count);
    if (!device.IsReadOnly()) {
        device.EnrichStatistics(statistics, CATEGORY_INFO, CACHE_MISS_WRITE_COUNT, cache_miss_write_count);
        device.EnrichStatistics(statistics, CATEGORY_ERROR, WRITE_ERROR_COUNT, write_error_count);
    }

    return statistics;
}
