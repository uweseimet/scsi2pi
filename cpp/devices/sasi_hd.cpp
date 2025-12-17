//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sasi_hd.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

SasiHd::SasiHd(int lun, const set<uint32_t> &sector_sizes) : Disk(SAHD, lun, false, false, sector_sizes)
{
    SetProductData( { "", "SASI HD", "" }, true);
    SetScsiLevel(ScsiLevel::NONE);
    SetProtectable(true);
}

void SasiHd::Open()
{
    assert(!IsReady());

    // Sector size (default 256 bytes) and number of sectors
    if (!SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 256)) {
        throw IoException("Invalid sector size");
    }
    SetBlockCount(static_cast<uint32_t>(GetFileSize() / GetBlockSize()));

    ValidateFile();
}

void SasiHd::Inquiry()
{
    // Byte 0 = 0: Direct access device

    const array<const uint8_t, 2> buf = { };
    GetController()->CopyToBuffer(buf.data(), buf.size());

    DataInPhase(buf.size());
}

vector<uint8_t> SasiHd::InquiryInternal() const
{
    assert(false);
    return {};
}

void SasiHd::RequestSense()
{
    // Transfer 4 bytes when size is 0 (SASI specification)
    int allocation_length = GetCdbByte(4);
    if (!allocation_length) {
        allocation_length = 4;
    }

    // Non-extended format
    const array<const uint8_t, 4> buf = { static_cast<uint8_t>(GetSenseKey()), static_cast<uint8_t>(GetLun() << 5) };
    GetController()->CopyToBuffer(buf.data(), allocation_length);

    DataInPhase(buf.size());
}
