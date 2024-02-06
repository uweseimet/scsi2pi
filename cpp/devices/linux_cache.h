//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "cache.h"

class LinuxCache : public Cache
{

public:

    LinuxCache(const string&, int, uint64_t, bool, bool);
    ~LinuxCache() override = default;

    bool ReadSectors(span<uint8_t>, uint64_t, uint32_t) override;
    bool WriteSector(span<const uint8_t>, uint64_t) override;

    int ReadLong(span<uint8_t>, uint64_t, int);
    int WriteLong(span<const uint8_t>, uint64_t, int);

    bool Init() override;

    bool Flush() override;

    vector<PbStatistics> GetStatistics(bool) const override;

private:

    string filename;

    fstream file;

    int sector_size;

    uint64_t sectors;

    bool write_through;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
};
