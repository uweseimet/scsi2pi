//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/interfaces/scsi_mmc_commands.h"
#include "disk.h"

class ScsiCd : public Disk, private ScsiMmcCommands
{

public:

    explicit ScsiCd(int, bool = false);
    ~ScsiCd() override = default;

    bool Init(const param_map&) override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;
    void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) override;
    int ReadData(span<uint8_t>) override;

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    struct Track
    {
        Track(int t, uint32_t f, uint32_t l) : track_no(t), first_lba(f), last_lba(l)
        {
        }

        bool IsValid(uint32_t lba) const
        {
            return lba >= first_lba && last_lba >= lba;
        }

        int track_no;
        uint32_t first_lba;
        uint32_t last_lba;
    };

    int ReadTocInternal(cdb_t, vector<uint8_t>&);

    void AddDeviceParametersPage(map<int, vector<byte>>&, bool) const;
    void AddAudioControlPage(map<int, vector<byte>>&, bool) const;

    void OpenIso();

    void CreateDataTrack();

    void ReadToc() override;

    void LBAtoMSF(uint32_t, uint8_t*) const;

    bool raw_file = false;

    // Track management
    void ClearTrack();
    int SearchTrack(uint32_t lba) const;
    vector<unique_ptr<Track>> tracks;
    int dataindex = -1;
};
