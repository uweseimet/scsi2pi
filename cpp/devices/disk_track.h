//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
//   Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <string>

using namespace std;

class DiskTrack
{
    struct
    {
        int track; // Track Number
        int size; // Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
        int sectors; // Number of sectors(<0x100)
        uint32_t length; // Data buffer length
        uint8_t *buffer; // Data buffer
        bool init; // Is it initilized?
        bool changed; // Changed flag
        vector<bool> changemap; // Changed map
    } dt = { };

public:

    DiskTrack() = default;
    ~DiskTrack();
    DiskTrack(DiskTrack&) = delete;
    DiskTrack& operator=(const DiskTrack&) = delete;

private:

    friend class DiskCache;

    void Init(int, int, int);
    bool Load(const string&, uint64_t&);
    bool Save(const string&, uint64_t&);

    int ReadSector(span<uint8_t>, int) const;
    int WriteSector(span<const uint8_t> buf, int);

    int GetTrack() const
    {
        return dt.track;
    }
};
