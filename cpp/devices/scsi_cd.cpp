//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_cd.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

ScsiCd::ScsiCd(int l, bool scsi1) : Disk(SCCD, l, true, false, { 512, 2048 })
{
    Disk::SetProductData( { "", "SCSI CD-ROM", "" }, true);
    SetScsiLevel(scsi1 ? ScsiLevel::SCSI_1_CCS : ScsiLevel::SCSI_2);
    SetProtectable(false);
    SetReadOnly(true);
    SetRemovable(true);
}

string ScsiCd::SetUp()
{
    AddCommand(ScsiCommand::READ_TOC, [this]
        {
            ReadToc();
        });

    return Disk::SetUp();
}

void ScsiCd::Open()
{
    assert(!IsReady());

    track_initialized = false;

    // Default sector size is 2048 bytes
    if (!SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 2048)) {
        throw IoException("Invalid sector size");
    }
    SetBlockCount(GetFileSize() / GetBlockSize());

    ValidateFile();

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

    const int track = GetCdbByte(6);

    // Track must be 1, except for lead out track ($AA)
    if (track > 1 && track != 0xaa) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    uint8_t track_number = 1;
    uint32_t track_address = first_lba;
    if (track && !track_initialized) {
        if (track != 0xaa) {
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
        }

        track_number = 0xaa;
        track_address = last_lba + 1;
    }

    const int length = min(GetCdbInt16(7), 12);
    auto &buf = GetController()->GetBuffer();
    fill_n(buf.data(), length, 0);

    // TOC data length, excluding this field itself
    SetInt16(buf, 0, 10);
    // First track number
    buf[2] = 1;
    // Last track number
    buf[3] = 1;
    // Data track, not audio track
    buf[5] = 0x14;
    buf[6] = track_number;

    // Track address in the requested format
    if (GetCdbByte(1) & 0x02) {
        LBAtoMSF(track_address, span(buf.data() + 8, buf.size() - 8));
    } else {
        SetInt16(buf, 10, track_address);
    }

    DataInPhase(length);
}

vector<uint8_t> ScsiCd::InquiryInternal() const
{
    return HandleInquiry(DeviceType::CD_DVD, true);
}

void ScsiCd::ModeSelect(cdb_t cdb, data_out_t buf, int offset, int length)
{
    Disk::ModeSelect(cdb, buf, offset, length);

    CreateDataTrack();
}

void ScsiCd::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    Disk::SetUpModePages(pages, page, changeable);

    if (page == 0x0d || page == 0x3f) {
        AddDeviceParametersPage(pages, changeable);
    }
}

void ScsiCd::AddDeviceParametersPage(map<int, vector<byte>> &pages, bool changeable)
{
    vector<byte> buf(8);

    if (!changeable) {
        // 2 seconds for inactive timer
        buf[3] = byte { 0x05 };

        // MSF multiples are 60 and 75 respectively
        buf[5] = byte { 60 };
        buf[7] = byte { 75 };
    }

    pages[13] = buf;
}

int ScsiCd::ReadData(data_in_t buf)
{
    CheckReady();

    if (const auto lba = static_cast<uint32_t>(GetNextSector()); first_lba > lba || last_lba < lba) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::LBA_OUT_OF_RANGE);
    }

    if (!track_initialized) {
        SetBlockCount(last_lba - first_lba + 1);

        if (!InitCache(GetFilename())) {
            throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
        }

        track_initialized = true;
    }

    return Disk::ReadData(buf);
}

void ScsiCd::LBAtoMSF(uint32_t lba, span<uint8_t> msf)
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
        ++m;
    }

    assert(m < 0x100);
    assert(s < 60);
    assert(f < 75);
    msf[0] = 0x00;
    msf[1] = static_cast<uint8_t>(m);
    msf[2] = static_cast<uint8_t>(s);
    msf[3] = static_cast<uint8_t>(f);
}
