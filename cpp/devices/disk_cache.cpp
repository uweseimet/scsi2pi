//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

#include <cstdlib>
#include <cassert>
#include <algorithm>
#include "disk_track.h"
#include "disk_cache.h"

DiskCache::DiskCache(const string &path, int size, uint32_t blocks)
: sec_path(path), sec_size(SHIFT_COUNTS.at(size)), sec_blocks(blocks)
{
    assert(blocks > 0);
}

bool DiskCache::Save()
{
    // Save valid tracks
    return ranges::none_of(cache.begin(), cache.end(), [this](const cache_t &c)
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

bool DiskCache::ReadSector(span<uint8_t> buf, uint32_t block)
{
    shared_ptr<DiskTrack> disktrk = GetTrack(block);
    if (!disktrk) {
        return false;
    }

    // Read the track data to the cache
    return disktrk->ReadSector(buf, block & 0xff);
}

bool DiskCache::WriteSector(span<const uint8_t> buf, uint32_t block)
{
    shared_ptr<DiskTrack> disktrk = GetTrack(block);
    if (!disktrk) {
        return false;
    }

    // Write the data to the cache
    return disktrk->WriteSector(buf, block & 0xff);
}

//---------------------------------------------------------------------------
//
//	Track Assignment
//
//---------------------------------------------------------------------------
shared_ptr<DiskTrack> DiskCache::Assign(int track)
{
    assert(sec_size != 0);
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
    for (size_t i = 0; i < cache.size(); i++) {
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
    for (size_t i = 0; i < cache.size(); i++) {
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

//---------------------------------------------------------------------------
//
//	Load cache
//
//---------------------------------------------------------------------------
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

    disktrk->Init(track, sec_size, sectors, cd_raw);

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
    serial++;
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
