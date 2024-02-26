//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <map>
#include <unordered_set>
#include "disk.h"

class ScsiHd : public Disk
{
    static constexpr string DEFAULT_PRODUCT = "SCSI HD";

public:

    ScsiHd(int, bool, bool, bool, const unordered_set<uint32_t>& = { 512, 1024, 2048, 4096 });
    ~ScsiHd() override = default;

    void FinalizeSetup();

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddVendorPages(map<int, vector<byte>>&, int, bool) const override;

private:

    string GetProductData() const;

    void AddFormatPage(map<int, vector<byte>>&, bool) const;
    void AddDrivePage(map<int, vector<byte>>&, bool) const;
    void AddNotchPage(map<int, vector<byte>>&, bool) const;
    void AddDecVendorPage(map<int, vector<byte>>&, bool) const;
};
