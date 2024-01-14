//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <span>
#include <vector>
#include <map>
#include <unordered_set>
#include "disk.h"

class ScsiHd : public Disk
{
    const string DEFAULT_PRODUCT = "SCSI HD";

public:

    ScsiHd(int, bool, bool, bool, const unordered_set<uint32_t>& = { 512, 1024, 2048, 4096 });
    ~ScsiHd() override = default;

    void FinalizeSetup(off_t);

    void Open() override;

    vector<uint8_t> InquiryInternal() override;
    void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) const override;

    void AddFormatPage(map<int, vector<byte>>&, bool) const override;

    void AddVendorModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddDecVendorModePage(map<int, vector<byte>>&, bool) const;

private:

    string GetProductData() const;

    scsi_defs::scsi_level scsi_level;
};
