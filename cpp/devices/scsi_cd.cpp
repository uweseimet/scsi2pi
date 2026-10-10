//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_cd.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"
#include "shared/file_util.h"
#include "shared/s2p_util.h"

using namespace file_util;
using namespace memory_util;
using namespace s2p_util;

ScsiCd::ScsiCd(int l, bool scsi1) : Disk(SCCD, l, true, false, { 512, 2048 })
{
    SetProductData( { "", "SCSI CD-ROM", "" }, true);
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

    // This call cannot fail, the method argument is always valid
    SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 2048);

    SetBlockCount(GetCapacityFromFile(GetFilename()) / GetBlockSize());

    ValidateFile();

    CreateDataTrack();

    if (IsReady()) {
        SetAttn(true);
    }
}

void ScsiCd::CreateDataTrack()
{
    first_lba = 0;
    last_lba = static_cast<uint32_t>(GetBlockCount()) - 1;
}

void ScsiCd::ReadToc()
{
    CheckReady();

    const int track = GetCdbByte(6);

    // The starting track must be 0 (first track), 1, or the lead-out track ($AA)
    if (track > 1 && track != 0xaa) {
        throw ScsiException(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
    }

    const bool msf = GetCdbByte(1) & 0x02;

    auto &buf = GetController()->GetBuffer();

    // Header (4 bytes) + at most two track descriptors (8 bytes each)
    fill_n(buf.data(), 20, 0);

    int offset = 4;

    const auto add_descriptor = [this, &buf, &offset, msf](uint8_t track_number, uint32_t address) {
        // ADR (position data in Q sub-channel) and CONTROL (data track)
        buf[offset + 1] = 0x14;
        buf[offset + 2] = track_number;
        if (msf) {
            // Convert logical blocks to 2048-byte frames, rounding up so that a lead-out address
            // that is not a multiple of the frame size is never reported too early
            const uint64_t frames = (static_cast<uint64_t>(address) * GetBlockSize() + 2048 - 1) / 2048;
            LBAtoMSF(static_cast<uint32_t>(min<uint64_t>(frames, UINT32_MAX)), span(buf.data() + offset + 4, 4));
        } else {
            // Track start address
            SetInt32(buf, offset + 4, address);
        }

        offset += 8;
    };

    // There is only one data track. It is returned unless only the lead-out was requested.
    if (track != 0xaa) {
        add_descriptor(1, first_lba);
    }

    // The lead-out track is always the last descriptor
    add_descriptor(0xaa, last_lba + 1);

    // TOC data length, excluding this field itself
    SetInt16(buf, 0, offset - 2);
    // First track number
    buf[2] = 1;
    // Last track number
    buf[3] = 1;

    DataInPhase(min(GetCdbInt16(7), offset));
}

void ScsiCd::ModeSelect(cdb_t cdb, data_out_t buf, int offset)
{
    Disk::ModeSelect(cdb, buf, offset);

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

void ScsiCd::LBAtoMSF(uint32_t lba, span<uint8_t> msf)
{
    // The base point is minutes=0, seconds=2, frames=0, i.e. an offset of 150 frames
    const uint64_t total_frames = static_cast<uint64_t>(lba) + 2 * 75;

    uint64_t minutes = total_frames / (60 * 75);
    uint64_t seconds = (total_frames / 75) % 60;
    uint64_t frames = total_frames % 75;

    // MSF cannot represent more than 255 minutes
    if (minutes > 0xff) {
        minutes = 0xff;
        seconds = 59;
        frames = 74;
    }

    msf[0] = 0x00;
    msf[1] = static_cast<uint8_t>(minutes);
    msf[2] = static_cast<uint8_t>(seconds);
    msf[3] = static_cast<uint8_t>(frames);
}
