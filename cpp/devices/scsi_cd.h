//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/interfaces/scsi_mmc_commands.h"
#include "disk.h"

class ScsiCd : public Disk, public ScsiMmcCommands
{

public:

    explicit ScsiCd(int, bool = false);
    ~ScsiCd() override = default;

    bool InitDevice() override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;
    void ModeSelect(cdb_t, span<const uint8_t>, int) override;
    int ReadData(span<uint8_t>) override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    void ReadToc() override;

    void CreateDataTrack();

    void AddDeviceParametersPage(map<int, vector<byte>>&, bool) const;

    static void LBAtoMSF(uint32_t, uint8_t*);

    uint32_t first_lba = 0;
    uint32_t last_lba = 0;

    bool track_initialized = false;
};
