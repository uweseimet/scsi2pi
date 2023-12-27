//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>
#include <map>
#include "base/interfaces/scsi_mmc_commands.h"
#include "cd_track.h"
#include "disk.h"

class ScsiCd : public Disk, private ScsiMmcCommands
{

public:

    explicit ScsiCd(int, bool = false);
    ~ScsiCd() override = default;

    bool Init(const param_map&) override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;
    void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) const override;
    int Read(span<uint8_t>, uint64_t) override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddFormatPage(map<int, vector<byte>>&, bool) const override;
    void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

    int ReadTocInternal(cdb_t, vector<uint8_t>&);

    void AddCDROMPage(map<int, vector<byte>>&, bool) const;
    void AddCDDAPage(map<int, vector<byte>>&, bool) const;
    scsi_defs::scsi_level scsi_level;

    void OpenIso();

    void CreateDataTrack();

    void ReadToc() override;

    void LBAtoMSF(uint32_t, uint8_t*) const; // LBA→MSF conversion

    bool rawfile = false; // RAW flag

    // Track management
    void ClearTrack(); // Clear the track
    int SearchTrack(uint32_t lba) const; // Track search
    vector<unique_ptr<CDTrack>> tracks; // Track opbject references
    int dataindex = -1; // Current data track
    int audioindex = -1; // Current audio track
};
