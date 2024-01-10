//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "mode_page_util.h"
#include "scsi_hd.h"

using namespace mode_page_util;

ScsiHd::ScsiHd(int lun, bool removable, bool apple, bool scsi1, const unordered_set<uint32_t> &sector_sizes)
: Disk(removable ? SCRM : SCHD, lun, sector_sizes), scsi_level(scsi1 ? scsi_level::scsi_1_ccs : scsi_level::scsi_2)
{
    // Some Apple tools require a particular drive identification.
    // Except for the vendor string .hda is the same as .hds.
    if (apple) {
        SetVendor("QUANTUM");
        SetProduct("FIREBALL");
    } else if (removable) {
        SetProduct("SCSI HD (REM.)");
    }
    SetProtectable(true);
    SetRemovable(removable);
    SetLockable(removable);
    SupportsSaveParameters(true);
}

string ScsiHd::GetProductData() const
{
    uint64_t capacity = GetBlockCount() * GetSectorSizeInBytes();
    string unit;

    // 10,000 MiB and more
    if (capacity >= 10'485'760'000) {
        capacity /= 1'073'741'824;
        unit = "GiB";
    }
    // 1 MiB and more
    else if (capacity >= 1'048'576) {
        capacity /= 1'048'576;
        unit = "MiB";
    }
    else {
        capacity /= 1024;
        unit = "KiB";
    }

    return DEFAULT_PRODUCT + " " + to_string(capacity) + " " + unit;
}

void ScsiHd::FinalizeSetup(off_t image_offset)
{
    Disk::ValidateFile();

    // For non-removable media drives set the default product name based on the drive capacity
    if (!IsRemovable()) {
        SetProduct(GetProductData(), false);
    }

    SetUpCache(image_offset);
}

void ScsiHd::Open()
{
    assert(!IsReady());

    const off_t size = GetFileSize();

    // Sector size (default 512 bytes) and number of blocks
    SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
    SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));

    FinalizeSetup(0);
}

vector<uint8_t> ScsiHd::InquiryInternal()
{
    return HandleInquiry(device_type::direct_access, scsi_level, IsRemovable());
}

void ScsiHd::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length) const
{
    if (const string result = mode_page_util::ModeSelect(cmd, cdb, buf, length, 1 << GetSectorSizeShiftCount());
    !result.empty()) {
        LogWarn(result);
    }
}

void ScsiHd::AddFormatPage(map<int, vector<byte>> &pages, bool changeable) const
{
    Disk::AddFormatPage(pages, changeable);

    EnrichFormatPage(pages, changeable, 1 << GetSectorSizeShiftCount());
}

void ScsiHd::AddVendorPage(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    // Page code 48
    if (page == 0x30 || page == 0x3f) {
        AddAppleVendorModePage(pages, changeable);
    }
}
