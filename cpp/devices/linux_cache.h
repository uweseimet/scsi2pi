//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
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

    LinuxCache(const string &f, int size, uint64_t s, bool w)
    : filename(f), sector_size(size), sectors(s), write_through(w)
    {
    }
    ~LinuxCache() override = default;

    int ReadSectors(data_in_t, uint64_t, uint32_t) override;
    int WriteSectors(data_out_t, uint64_t, uint32_t) override;

    int ReadLong(data_in_t, uint64_t, int);
    int WriteLong(data_out_t, uint64_t, int);

    bool Init() override;

    bool Flush() override;

    vector<PbStatistics> GetStatistics(const Device&) const override;

private:

    int Read(data_in_t, uint64_t, int);
    int Write(data_out_t, uint64_t, int);

    string filename;

    fstream file;

    int sector_size;

    uint64_t sectors;

    bool write_through;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
};
