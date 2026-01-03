//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"

class OpticalMemory : public Disk
{

public:

    explicit OpticalMemory(int);
    ~OpticalMemory() override = default;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    void AddOptionPage(map<int, vector<byte>>&) const;
    void AddVendorPage(map<int, vector<byte>>&, bool) const;

    // Mapping of some typical real device capacities to sector sizes and sector counts
    inline static const unordered_map<uint64_t, const pair<uint32_t, uint32_t>> GEOMETRIES = {
        // 128 MiB, 512 bytes per sector, 248826 sectors
        { 512 * 248826, { 512, 248826 } },
        // 230 MiB, 512 bytes per sector, 446325 sectors
        { 512 * 446325, { 512, 446325 } },
        // 540 MiB, 512 bytes per sector, 1041500 sectors
        { 512 * 1041500, { 512, 1041500 } },
        // 640 MiB, 20248 bytes per sector, 310352 sectors
        { 2048 * 310352, { 2048, 310352 } } };
};
