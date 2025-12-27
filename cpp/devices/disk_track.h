//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
//   Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include "shared/s2p_defs.h"

class DiskTrack final
{

public:

    DiskTrack() = default;
    ~DiskTrack() = default;
    DiskTrack(DiskTrack&) = delete;
    DiskTrack& operator=(const DiskTrack&) = delete;

private:

    friend class DiskCache;

    void Init(int64_t, int, int);
    bool Load(const string&, uint64_t&);
    bool Save(const string&, uint64_t&);

    int ReadSector(data_in_t, int) const;
    int WriteSector(data_out_t, int);

    auto GetTrack() const
    {
        return track_number;
    }

    int64_t track_number = 0;

    // 8 = 256, 9 = 512, 10 = 1024, 11 = 2048, 12 = 4096
    int shift_count = 0;

    // < 256
    int sector_count = 0;

    vector<uint8_t> unaligned_buffer;

    uint8_t *buffer = nullptr;

    bool is_initialized = false;

    bool is_modified = false;

    // Do not use bool here in order to avoid special rules for vector<bool>
    vector<uint8_t> modified_flags;
};
