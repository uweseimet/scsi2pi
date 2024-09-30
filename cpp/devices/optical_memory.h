//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"

using Geometry = pair<uint32_t, uint32_t>;

class OpticalMemory : public Disk
{
public:

    explicit OpticalMemory(int);
    ~OpticalMemory() override = default;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    void AddOptionPage(map<int, vector<byte>>&) const;
    void AddVendorPage(map<int, vector<byte>>&, bool) const;

    bool SetGeometryForCapacity(uint64_t);

    // The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
    unordered_map<uint64_t, Geometry> geometries;
};
