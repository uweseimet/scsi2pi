//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"

class ScsiCd : public Disk
{

public:

    ScsiCd(int, bool);
    ~ScsiCd() override = default;

    string SetUp() override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;
    void ModeSelect(cdb_t, data_out_t, int) override;
    int ReadData(data_in_t) override;

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    void ReadToc();

    void CreateDataTrack();

    static void AddDeviceParametersPage(map<int, vector<byte>>&, bool);

    static void LBAtoMSF(uint32_t, span<uint8_t>);

    uint32_t first_lba = 0;
    uint32_t last_lba = 0;

    bool track_initialized = false;
};
