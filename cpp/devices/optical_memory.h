//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <span>
#include <vector>
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
    void AddVendorPages(map<int, vector<byte>>&, int, bool) const override;

private:

    void AddOptionPage(map<int, vector<byte>>&, bool) const;

    bool SetGeometryForCapacity(uint64_t);

    // The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
    unordered_map<uint64_t, Geometry> geometries;
};
