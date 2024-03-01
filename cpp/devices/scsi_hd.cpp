//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "scsi_hd.h"

using namespace memory_util;

ScsiHd::ScsiHd(int lun, bool removable, bool apple, bool scsi1, const unordered_set<uint32_t> &sector_sizes)
: Disk(removable ? SCRM : SCHD, scsi1 ? scsi_level::scsi_1_ccs : scsi_level::scsi_2, lun, true, sector_sizes)
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

void ScsiHd::FinalizeSetup()
{
    Disk::ValidateFile();

    // For non-removable media drives set the default product name based on the drive capacity
    if (!IsRemovable()) {
        SetProduct(GetProductData(), false);
    }
}

void ScsiHd::Open()
{
    assert(!IsReady());

    // Sector size (default 512 bytes) and number of blocks
    if (!SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512)) {
        throw io_exception("Invalid sector size");
    }
    SetBlockCount(static_cast<uint32_t>(GetFileSize() / GetSectorSizeInBytes()));

    FinalizeSetup();
}

vector<uint8_t> ScsiHd::InquiryInternal() const
{
    return HandleInquiry(device_type::direct_access, IsRemovable());
}

void ScsiHd::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    Disk::SetUpModePages(pages, page, changeable);

    // Page 3 (format device)
    if (page == 0x03 || page == 0x3f) {
        AddFormatPage(pages, changeable);
    }

    // Page 4 (rigid drive page)
    if (page == 0x04 || page == 0x3f) {
        AddDrivePage(pages, changeable);
    }

    // Page 12 (notch)
    if (page == 0x0c || page == 0x3f) {
        AddNotchPage(pages, changeable);
    }
}

void ScsiHd::AddFormatPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(24);

    if (changeable) {
        // The sector size is simulated to be changeable in multiples of 4,
        // see the MODE SELECT implementation for details
        SetInt16(buf, 12, 0xffff);

        pages[3] = buf;

        return;
    }

    if (IsReady()) {
        // Set the number of tracks in one zone to 8
        buf[3] = (byte)0x08;

        // Set sector/track to 25
        SetInt16(buf, 10, 25);

        // The current sector size
        SetInt16(buf, 12, GetSectorSizeInBytes());

        // Interleave 1
        SetInt16(buf, 14, 1);

        // Track skew factor 11
        SetInt16(buf, 16, 11);

        // Cylinder skew factor 20
        SetInt16(buf, 18, 20);
    }

    buf[20] = IsRemovable() ? (byte)0x20 : (byte)0x00;

    // Hard-sectored
    buf[20] |= (byte)0x40;

    pages[3] = buf;
}

void ScsiHd::AddDrivePage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(24);

    // No changeable area
    if (changeable) {
        pages[4] = buf;

        return;
    }

    if (IsReady()) {
        // Set the number of cylinders (total number of blocks
        // divided by 25 sectors/track and 8 heads)
        uint64_t cylinders = GetBlockCount();
        cylinders >>= 3;
        cylinders /= 25;
        SetInt32(buf, 0x01, static_cast<uint32_t>(cylinders));

        // Fix the head at 8
        buf[0x05] = (byte)0x8;

        // Medium rotation rate 7200
        SetInt16(buf, 0x14, 7200);
    }

    pages[4] = buf;
}

void ScsiHd::AddNotchPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(24);

    // Not having a notched drive (i.e. not setting anything) probably provides the best compatibility

    pages[12] = buf;
}

void ScsiHd::AddVendorPages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    // Page code 37
    if (page == 0x25 || page == 0x3f) {
        AddDecVendorPage(pages, changeable);
    }
}

// See https://manx-docs.org/collections/antonio/dec/dec-scsi.pdf
void ScsiHd::AddDecVendorPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(25);

    // buf[2] bit 0 is the Spin-up Disable (SPD) bit, if 1 the drive will not spin up on initial power up

    pages[0x25] = buf;
}
