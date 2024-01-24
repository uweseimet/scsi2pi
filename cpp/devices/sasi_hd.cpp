//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/scsi.h"
#include "disk.h"
#include "sasi_hd.h"

SasiHd::SasiHd(int lun, const unordered_set<uint32_t> &sector_sizes) : Disk(SAHD, lun, false, sector_sizes)
{
    SetProduct("SASI HD");
    SetProtectable(true);
}

void SasiHd::FinalizeSetup(off_t image_offset)
{
    Disk::ValidateFile();

    SetUpCache(image_offset);
}

void SasiHd::Open()
{
    assert(!IsReady());

    const off_t size = GetFileSize();

    // Sector size (default 256 bytes) and number of blocks
    SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 256);
    SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));

    FinalizeSetup(0);
}

void SasiHd::Inquiry()
{
    // Byte 0 = 0: Direct access device

    array<uint8_t, 2> buf = { };
    GetController()->CopyToBuffer(buf.data(), buf.size());

    EnterDataInPhase();
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
    array<uint8_t, 4> buf = { static_cast<uint8_t>(GetStatusCode() >> 16), static_cast<uint8_t>(GetLun() << 5) };
    GetController()->CopyToBuffer(buf.data(), buf.size());

    EnterDataInPhase();
}
