//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_hd.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

ScsiHd::ScsiHd(int l, bool r, bool apple, bool scsi1, const set<uint32_t> &sector_sizes)
: Disk(r ? SCRM : SCHD, l, true, true, sector_sizes)
{
    // Some Apple tools require a particular drive identification.
    // Except for the vendor string .hda is the same as .hds.
    if (apple) {
        SetProductData( { "QUANTUM", "FIREBALL", "" }, true);
    } else if (r) {
        SetProductData( { "", "SCSI HD (SCRM)", "" }, true);
    }
    SetScsiLevel(scsi1 ? ScsiLevel::SCSI_1_CCS : ScsiLevel::SCSI_2);
    SetProtectable(true);
    SetRemovable(r);
}

void ScsiHd::FinalizeSetup()
{
    ValidateFile();

    // For non-removable media drives set the default product name based on the drive capacity
    if (!IsRemovable()) {
        const uint64_t capacity = GetBlockCount() * GetBlockSize();

        const auto *unit = ranges::find_if(UNITS, [capacity](const Unit &u) {return capacity >= u.threshold;});

        SetProductData( { "", fmt::format("SCSI HD {0} {1}iB", capacity / unit->divisor, unit->abbr), "" }, false);
    }
}

void ScsiHd::Open()
{
    assert(!IsReady());

    // This call cannot fail, the method argument is always valid
    SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 512);

    SetBlockCount(GetFileSize() / GetBlockSize());

    FinalizeSetup();
}

vector<uint8_t> ScsiHd::InquiryInternal() const
{
    return HandleInquiry(DeviceType::DIRECT_ACCESS, IsRemovable());
}

bool ScsiHd::ValidateBlockSize(uint32_t size) const
{
    // Non-removable hard drives support multiples of 4
    return IsRemovable() ? Disk::ValidateBlockSize(size) : size && !(size % 4);
}

void ScsiHd::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    Disk::SetUpModePages(pages, page, changeable);

    // Page 3 (format device)
    if (page == 0x03 || page == 0x3f) {
        AddFormatPage(pages, changeable);
    }

    // Page 4 (rigid drive)
    if (page == 0x04 || page == 0x3f) {
        AddDrivePage(pages, changeable);
    }

    // Page 12 (notch)
    if (page == 0x0c || page == 0x3f) {
        AddNotchPage(pages, changeable);
    }

    if (page == 0x25 || page == 0x3f) {
        AddDecVendorPage(pages, changeable);
    }
}

void ScsiHd::AddFormatPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(24);

    if (changeable) {
        // The sector size is simulated to be changeable in multiples of 4.
        // See the MODE SELECT implementation for details.
        SetInt16(buf, 12, 0x1ffc);

        pages[3] = buf;

        return;
    }

    if (IsReady()) {
        // 8 tracks in one zone
        buf[3] = byte { 0x08 };

        // 25 sectors/tracks
        SetInt16(buf, 10, 25);

        // The current sector size
        SetInt16(buf, 12, GetBlockSize());

        // Interleave 1
        SetInt16(buf, 14, 1);

        // Track skew factor 11
        SetInt16(buf, 16, 11);

        // Cylinder skew factor 20
        SetInt16(buf, 18, 20);
    }

    buf[20] = IsRemovable() ? byte { 0x20 } : byte { 0x00 };

    // Hard-sectored
    buf[20] |= byte { 0x40 };

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

        // 8 heads
        buf[0x05] = byte { 0x8 };

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

// See https://manx-docs.org/collections/antonio/dec/dec-scsi.pdf
void ScsiHd::AddDecVendorPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(25);

    // buf[2] bit 0 is the Spin-up Disable (SPD) bit, if 1 the drive will not spin up on initial power up

    pages[0x25] = buf;
}
