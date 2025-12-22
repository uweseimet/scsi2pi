//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"

class ScsiHd : public Disk
{

public:

    ScsiHd(int, bool, bool, bool, const set<uint32_t>& = { 512, 1024, 2048, 4096 });
    ~ScsiHd() override = default;

    void FinalizeSetup();

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

    bool ValidateBlockSize(uint32_t) const override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    void AddFormatPage(map<int, vector<byte>>&, bool) const;
    void AddDrivePage(map<int, vector<byte>>&, bool) const;
    void AddNotchPage(map<int, vector<byte>>&, bool) const;
    void AddDecVendorPage(map<int, vector<byte>>&, bool) const;

    struct Unit
    {
        uint64_t threshold;
        uint64_t divisor;
        char abbr;
    };

    inline static constexpr array<Unit, 4> UNITS = { {
        { 10'737'418'240'000, 1'099'511'627'776, 'T' },
        { 10'485'760'000, 1'073'741'824, 'G' },
        { 1'048'576, 1'048'576, 'M' },
        { 0, 1014, 'K' }
    } };
};
