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
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "disk_cache.h"
#include <cassert>
#include "disk_track.h"

DiskCache::DiskCache(const string &path, int size, uint64_t sectors) : sec_path(path), sec_blocks(
    static_cast<int>(sectors))
{
    while ((1 << sec_size) != size) {
        ++sec_size;
    }
    assert(sec_size >= 8 && sec_size <= 12);
}

bool DiskCache::Init()
{
    return sec_blocks && !sec_path.empty();
}

bool DiskCache::Flush()
{
    // Save valid tracks
    return ranges::none_of(cache, [this](const cache_t &c)
        {   return c.disktrk && !c.disktrk->Save(sec_path, cache_miss_write_count);});
}

shared_ptr<DiskTrack> DiskCache::GetTrack(uint32_t block)
{
    // Update first
    UpdateSerialNumber();

    // Calculate track (fixed to 256 sectors/track)
    int track = block >> 8;

    // Get track data
    return Assign(track);
}

int DiskCache::ReadSectors(data_in_t buf, uint64_t sector, uint32_t count)
{
    assert(count == 1);
    if (count != 1) {
        return 0;
    }

    shared_ptr<DiskTrack> disktrk = GetTrack(static_cast<uint32_t>(sector));
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

    shared_ptr<DiskTrack> disktrk = GetTrack(static_cast<uint32_t>(sector));
    if (!disktrk) {
        return 0;
    }

    // Write the data to the cache
    return disktrk->WriteSector(buf, sector & 0xff);
}

// Track Assignment
shared_ptr<DiskTrack> DiskCache::Assign(int track)
{
    assert(track >= 0);

    // First, check if it is already assigned
    for (cache_t &c : cache) {
        if (c.disktrk && c.disktrk->GetTrack() == track) {
            // Track match
            c.serial = serial;
            return c.disktrk;
        }
    }

    // Next, check for empty
    for (size_t i = 0; i < cache.size(); ++i) {
        if (!cache[i].disktrk) {
            // Try loading
            if (Load(static_cast<int>(i), track, nullptr)) {
                // Success loading
                cache[i].serial = serial;
                return cache[i].disktrk;
            }

            // Load failed
            return nullptr;
        }
    }

    // Finally, find the youngest serial number and delete it

    // Set index 0 as candidate c
    uint32_t s = cache[0].serial;
    size_t c = 0;

    // Compare candidate with serial and update to smaller one
    for (size_t i = 0; i < cache.size(); ++i) {
        assert(cache[i].disktrk);

        // Compare and update the existing serial
        if (cache[i].serial < s) {
            s = cache[i].serial;
            c = i;
        }
    }

    // Save this track
    if (!cache[c].disktrk->Save(sec_path, cache_miss_write_count)) {
        return nullptr;
    }

    // Delete this track
    shared_ptr<DiskTrack> disktrk = cache[c].disktrk;
    cache[c].disktrk.reset();

    if (Load(static_cast<int>(c), track, disktrk)) {
        // Successful loading
        cache[c].serial = serial;
        return cache[c].disktrk;
    }

    // Load failed
    return nullptr;
}

bool DiskCache::Load(int index, int track, shared_ptr<DiskTrack> disktrk)
{
    assert(index >= 0 && index < static_cast<int>(cache.size()));
    assert(track >= 0);
    assert(!cache[index].disktrk);

    // Get the number of sectors on this track
    int sectors = sec_blocks - (track << 8);
    assert(sectors > 0);
    if (sectors > 0x100) {
        sectors = 0x100;
    }

    if (!disktrk) {
        disktrk = make_shared<DiskTrack>();
    }

    disktrk->Init(track, sec_size, sectors);

    // Try loading
    if (!disktrk->Load(sec_path, cache_miss_read_count)) {
        ++read_error_count;

        return false;
    }

    // Allocation successful, work set
    cache[index].disktrk = disktrk;

    return true;
}

void DiskCache::UpdateSerialNumber()
{
    // Update and do nothing except 0
    ++serial;
    if (serial != 0) {
        return;
    }

    // Clear serial of all caches
    for (cache_t &c : cache) {
        c.serial = 0;
    }
}

vector<PbStatistics> DiskCache::GetStatistics(bool is_read_only) const
{
    vector<PbStatistics> statistics;

    PbStatistics s;

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(CACHE_MISS_READ_COUNT);
    s.set_value(cache_miss_read_count);
    statistics.push_back(s);

    if (!is_read_only) {
        s.set_key(CACHE_MISS_WRITE_COUNT);
        s.set_value(cache_miss_write_count);
        statistics.push_back(s);
    }

    s.set_category(PbStatisticsCategory::CATEGORY_ERROR);

    s.set_key(READ_ERROR_COUNT);
    s.set_value(read_error_count);
    statistics.push_back(s);

    if (!is_read_only) {
        s.set_key(WRITE_ERROR_COUNT);
        s.set_value(write_error_count);
        statistics.push_back(s);
    }

    return statistics;
}
