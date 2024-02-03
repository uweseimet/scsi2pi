//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <fstream>
#include "cache.h"

class NullCache : public Cache
{

public:

    NullCache(const string&, int, uint32_t, bool);
    ~NullCache() override = default;

    bool ReadSector(span<uint8_t>, uint32_t) override;
    bool WriteSector(span<const uint8_t>, uint32_t) override;

    bool Init() override;

    bool Flush() override;

    vector<PbStatistics> GetStatistics(bool) const override;

private:

    string filename;

    fstream file;

    int sector_size;

    uint32_t sectors;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;
};
