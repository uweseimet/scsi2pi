//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "scsi_cd.h"

using namespace memory_util;

ScsiCd::ScsiCd(int lun, bool scsi1) : Disk(SCCD, scsi1 ? scsi_level::scsi_1_ccs : scsi_level::scsi_2, lun, true, false,
    { 512, 2048 })
{
    SetProduct("SCSI CD-ROM");
    SetReadOnly(true);
    SetRemovable(true);
    SetLockable(true);
}

bool ScsiCd::Init(const param_map &params)
{
    Disk::Init(params);

    AddCommand(scsi_command::cmd_read_toc, [this]
        {
            ReadToc();
        });

    return true;
}

void ScsiCd::Open()
{
    assert(!IsReady());

    track_initialized = false;

    // Default sector size is 2048 bytes
    if (!SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 2048)) {
        throw io_exception("Invalid sector size");
    }
    SetBlockCount(GetFileSize() / GetSectorSizeInBytes());

    Disk::ValidateFile();

    SetReadOnly(true);
    SetProtectable(false);

    CreateDataTrack();

    if (IsReady()) {
        SetAttn(true);
    }
}

void ScsiCd::CreateDataTrack()
{
    first_lba = 0;
    last_lba = static_cast<int>(GetBlockCount()) - 1;
    track_initialized = true;
}

void ScsiCd::ReadToc()
{
    CheckReady();

    const auto &cdb = GetController()->GetCdb();

    // Track must be 1, except for lead out track ($AA)
    if (cdb[6] > 1 && cdb[6] != 0xaa) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    uint8_t track_number = 1;
    uint32_t track_address = first_lba;
    if (cdb[6] && !track_initialized) {
        if (cdb[6] != 0xaa) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
        }

        track_number = 0xaa;
        track_address = last_lba + 1;
    }

    const int length = min(GetInt16(cdb, 7), 12);
    auto &buf = GetController()->GetBuffer();
    fill_n(buf.data(), length, 0);

    // TOC data length, excluding this field itself
    SetInt16(buf, 0, 10);
    // First track number
    buf[2] = 1;
    // Last track number
    buf[3] = 1;

    buf[6] = track_number;

    // Track address in the requested format (MSF)
    if (cdb[1] & 0x02) {
        LBAtoMSF(track_address + 1, &buf[8]);
    } else {
        SetInt16(buf, 10, track_address + 1);
    }

    DataInPhase(length);
}

vector<uint8_t> ScsiCd::InquiryInternal() const
{
    return HandleInquiry(device_type::cd_rom, true);
}

void ScsiCd::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length)
{
    Disk::ModeSelect(cmd, cdb, buf, length);

    CreateDataTrack();
}

void ScsiCd::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    Disk::SetUpModePages(pages, page, changeable);

    if (page == 0x0d || page == 0x3f) {
        AddDeviceParametersPage(pages, changeable);
    }
}

void ScsiCd::AddDeviceParametersPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(8);

    // No changeable area
    if (!changeable) {
        // 2 seconds for inactive timer
        buf[3] = (byte)0x05;

        // MSF multiples are 60 and 75 respectively
        buf[5] = (byte)60;
        buf[7] = (byte)75;
    }

    pages[13] = buf;
}

int ScsiCd::ReadData(span<uint8_t> buf)
{
    CheckReady();

    if (const auto lba = static_cast<uint32_t>(GetNextSector()); first_lba > lba || last_lba < lba) {
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    }

    if (!track_initialized) {
        SetBlockCount(last_lba - first_lba + 1);

        if (!InitCache(GetFilename())) {
            throw scsi_exception(sense_key::medium_error, asc::read_fault);
        }

        track_initialized = true;
    }

    return Disk::ReadData(buf);
}

void ScsiCd::LBAtoMSF(uint32_t lba, uint8_t *msf)
{
    // 75 and 75*60 get the remainder
    uint32_t m = lba / (75 * 60);
    uint32_t s = lba % (75 * 60);
    const uint32_t f = s % 75;
    s /= 75;

    // The base point is M=0, S=2, F=0
    s += 2;
    if (s >= 60) {
        s -= 60;
        m++;
    }

    assert(m < 0x100);
    assert(s < 60);
    assert(f < 75);
    msf[0] = 0x00;
    msf[1] = static_cast<uint8_t>(m);
    msf[2] = static_cast<uint8_t>(s);
    msf[3] = static_cast<uint8_t>(f);
}
