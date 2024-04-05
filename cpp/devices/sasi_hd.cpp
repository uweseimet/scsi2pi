//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/s2p_exceptions.h"
#include "sasi_hd.h"

SasiHd::SasiHd(int lun, const unordered_set<uint32_t> &sector_sizes) : Disk(SAHD, scsi_level::none, lun, false, false,
    sector_sizes)
{
    SetProduct("SASI HD");
    SetProtectable(true);
}

void SasiHd::Open()
{
    assert(!IsReady());

    // Sector size (default 256 bytes) and number of blocks
    if (!SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 256)) {
        throw io_exception("Invalid sector size");
    }
    SetBlockCount(static_cast<uint32_t>(GetFileSize() / GetSectorSizeInBytes()));

    Disk::ValidateFile();
}

void SasiHd::Inquiry()
{
    // Byte 0 = 0: Direct access device

    const array<uint8_t, 2> buf = { };
    GetController()->CopyToBuffer(buf.data(), buf.size());

    DataInPhase(buf.size());
}

vector<uint8_t> SasiHd::InquiryInternal() const
{
    assert(false);
    return vector<uint8_t>();
}

void SasiHd::RequestSense()
{
    // Transfer 4 bytes when size is 0 (Shugart Associates System Interface specification)
    //vector<uint8_t> buf(allocation_length ? allocation_length : 4);

    // SASI fixed to non-extended format
    const array<uint8_t, 4> buf = { static_cast<uint8_t>(GetSenseKey()), static_cast<uint8_t>(GetLun() << 5) };
    GetController()->CopyToBuffer(buf.data(), buf.size());

    DataInPhase(buf.size());
}
