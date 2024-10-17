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

#include "disk_track.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <spdlog/spdlog.h>

DiskTrack::~DiskTrack()
{
    // Release memory, but do not save automatically
    free(dt.buffer); // NOSONAR free() must be used here because of allocation with posix_memalign
}

void DiskTrack::Init(int track, int size, int sectors)
{
    assert(track >= 0);
    assert(sectors > 0 && sectors <= 256);

    dt.track = track;
    dt.size = size;
    dt.sectors = sectors;
    dt.init = false;
    dt.changed = false;
}

bool DiskTrack::Load(const string &path, uint64_t &cache_miss_read_count)
{
    // Not needed if already loaded
    if (dt.init) {
        assert(dt.buffer);
        return true;
    }

    ++cache_miss_read_count;

    // Calculate offset (previous tracks are considered to hold 256 sectors)
    off_t offset = ((off_t)dt.track << 8);
    offset <<= dt.size;

    // Calculate length (data size of this track)
    const int length = dt.sectors << dt.size;

    // Allocate buffer memory
    if (!dt.buffer && !posix_memalign((void**)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
        dt.length = length;
    }

    // Reallocate if the buffer length is different
    if (dt.buffer && dt.length != static_cast<uint32_t>(length)) {
        free(dt.buffer); // NOSONAR free() must be used here because of allocation with posix_memalign
        dt.buffer = nullptr;
        if (!posix_memalign((void**)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
            dt.length = length;
        }
    }

    if (!dt.buffer) {
        return false;
    }

    // Resize and clear changemap
    dt.changemap.resize(dt.sectors);
    fill(dt.changemap.begin(), dt.changemap.end(), false); // NOSONAR ranges::fill() cannot be applied to vector<bool>

    ifstream in(path, ios::binary);
    if (in.fail()) {
        in.clear();
        return false;
    }

    in.seekg(offset);
    if (in.fail()) {
        in.clear();
        return false;
    }
    in.read((char*)dt.buffer, length);
    if (in.fail()) {
        in.clear();
        return false;
    }

    // Set a flag and end normally
    dt.init = true;
    dt.changed = false;
    return true;
}

bool DiskTrack::Save(const string &path, uint64_t &cache_miss_write_count)
{
    if (!dt.init) {
        return true;
    }

    if (!dt.changed) {
        return true;
    }
    // Need to write
    assert(dt.buffer);

    ++cache_miss_write_count;

    // Calculate offset (previous tracks are considered to hold 256 sectors)
    off_t offset = ((off_t)dt.track << 8);
    offset <<= dt.size;

    // Calculate length per sector
    const int length = 1 << dt.size;

    ofstream out(path, ios::in | ios::out | ios::binary);
    if (out.fail()) {
        out.clear();
        return false;
    }

    // Partial write loop
    int total;
    for (int i = 0; i < dt.sectors;) {
        // If changed
        if (dt.changemap[i]) {
            // Initialize write size
            total = 0;

            out.seekp(offset + (i << dt.size));
            if (out.fail()) {
                out.clear();
                return false;
            }

            // Consectutive sector length
            int j;
            for (j = i; j < dt.sectors; j++) {
                // end when interrupted
                if (!dt.changemap[j]) {
                    break;
                }

                // Add one sector
                total += length;
            }

            out.write((const char*)&dt.buffer[i << dt.size], total);
            if (out.fail()) {
                out.clear();
                return false;
            }

            // To unmodified sector
            i = j;
        } else {
            // Next Sector
            i++;
        }
    }

    // Drop the change flag and exit
    fill(dt.changemap.begin(), dt.changemap.end(), false); // NOSONAR ranges::fill() cannot be applied to vector<bool>
    dt.changed = false;

    return true;
}

int DiskTrack::ReadSector(span<uint8_t> buf, int sec) const
{
    assert(sec >= 0 && sec < 256);

    if (!dt.init) {
        return 0;
    }

    if (sec >= dt.sectors) {
        return 0;
    }

    const int length = 1 << dt.size;

    // Copy
    assert(dt.buffer);
    memcpy(buf.data(), &dt.buffer[(off_t)sec << dt.size], length);

    return length;
}

int DiskTrack::WriteSector(span<const uint8_t> buf, int sec)
{
    assert(sec >= 0 && sec < 256);

    if (!dt.init) {
        return 0;
    }

    if (sec >= dt.sectors) {
        return 0;
    }

    const int offset = sec << dt.size;
    const int length = 1 << dt.size;

    // Compare
    assert(dt.buffer);
    if (!memcmp(buf.data(), &dt.buffer[offset], length)) {
        // Exit normally since it's attempting to write the same thing
        return length;
    }

    // Copy, change
    memcpy(&dt.buffer[offset], buf.data(), length);
    dt.changemap[sec] = true;
    dt.changed = true;

    return length;
}
