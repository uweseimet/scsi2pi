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

#include "disk_track.h"
#include <cassert>
#include <fstream>
#include <spdlog/spdlog.h>

DiskTrack::~DiskTrack()
{
    free(buffer); // NOSONAR free() must be used here due to posix_memalign
}

void DiskTrack::Init(int64_t track, int size, int sectors)
{
    assert(track >= 0);
    assert(sectors > 0 && sectors <= 256);

    track_number = track;
    shift_count = size;
    sector_count = sectors;
    is_initialized = false;
    is_modified = false;
}

bool DiskTrack::Load(const string &path, uint64_t &cache_miss_read_count)
{
    // Not needed if already loaded
    if (is_initialized) {
        assert(buffer);
        return true;
    }

    ++cache_miss_read_count;

    const uint64_t size = sector_count << shift_count;

    // Allocate or reallocate the buffer
    if (!buffer || buffer_size != size) {
        free(buffer); // NOSONAR free() must be used here due to posix_memalign
        buffer = nullptr;

        if (posix_memalign((void**)&buffer, 512, (size + 511) & ~511)) {
            return false;
        }

        buffer_size = size;
    }

    modified_flags.resize(sector_count);
    ranges::fill(modified_flags, 0);
    is_initialized = true;
    is_modified = false;

    ifstream in(path, ios::binary);
    if (in.fail()) {
        return false;
    }

    // Calculate offset (previous tracks are considered to hold 256 sectors)
    off_t offset = track_number << 8;
    offset <<= shift_count;

    in.seekg(offset);
    in.read(reinterpret_cast<char*>(buffer), size);
    return in.good();
}

bool DiskTrack::Save(const string &path, uint64_t &cache_miss_write_count)
{
    if (!is_initialized || !is_modified) {
        return true;
    }

    assert(buffer);

    ++cache_miss_write_count;

    // Calculate offset (previous tracks are considered to hold 256 sectors)
    off_t offset = track_number << 8;
    offset <<= shift_count;

    const int size = 1 << shift_count;

    // ios:in is required in order not to truncate
    ofstream out(path, ios::in | ios::out | ios::binary);
    if (out.fail()) {
        return false;
    }

    // Write consecutive sectors
    int i = 0;
    while (i < sector_count) {
        if (modified_flags[i]) {
            int total = 0;

            // Determine consecutive sector range
            int j = i;
            while (j < sector_count && modified_flags[j]) {
                total += size;
                ++j;
            }

            out.seekp(offset + (i << shift_count));
            out.write(reinterpret_cast<const char*>(buffer) + (i << shift_count), total);
            if (out.fail()) {
                return false;
            }

            // Next unmodified sector
            i = j;
        } else {
            ++i;
        }
    }

    ranges::fill(modified_flags, 0);
    is_modified = false;

    return true;
}

int DiskTrack::ReadSector(data_in_t buf, int sector) const
{
    assert(sector >= 0 && sector < 256);

    if (!is_initialized || sector >= sector_count) {
        return 0;
    }

    assert(buffer);

    const int size = 1 << shift_count;

    memcpy(buf.data(), buffer + (static_cast<off_t>(sector) << shift_count), size);

    return size;
}

int DiskTrack::WriteSector(data_out_t buf, int sector)
{
    assert(sector >= 0 && sector < 256);

    if (!is_initialized || sector >= sector_count) {
        return 0;
    }

    assert(buffer);

    const int offset = sector << shift_count;
    const int size = 1 << shift_count;

    // Check if any data have changed
    if (memcmp(buf.data(), buffer + offset, size)) {
        memcpy(buffer + offset, buf.data(), size);
        modified_flags[sector] = 1;
        is_modified = true;
    }

    return size;
}
