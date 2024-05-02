//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

void ScsiMo_SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(8U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(4U, pages[6].size());
    EXPECT_EQ(12U, pages[7].size());
    EXPECT_EQ(12U, pages[8].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(12U, pages[32].size());
}

TEST(OpticalMemoryTest, Inquiry)
{
    TestShared::Inquiry(SCMO, device_type::optical_memory, scsi_level::scsi_2, "SCSI2Pi SCSI MO         ", 0x1f, true);
}

TEST(OpticalMemoryTest, GetSectorSizes)
{
    OpticalMemory mo(0);

    const auto &sector_sizes = mo.GetSupportedSectorSizes();
    EXPECT_EQ(4U, sector_sizes.size());

    EXPECT_TRUE(sector_sizes.contains(512));
    EXPECT_TRUE(sector_sizes.contains(1024));
    EXPECT_TRUE(sector_sizes.contains(2048));
    EXPECT_TRUE(sector_sizes.contains(4096));
}

TEST(OpticalMemoryTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockOpticalMemory mo(0);

    // Non changeable
    mo.SetUpModePages(pages, 0x3f, false);
    ScsiMo_SetUpModePages(pages);

    // Changeable
    pages.clear();
    mo.SetUpModePages(pages, 0x3f, true);
    ScsiMo_SetUpModePages(pages);
}

TEST(OpticalMemoryTest, AddVendorPages)
{
    map<int, vector<byte>> pages;
    MockOpticalMemory mo(0);

    mo.SetReady(true);
    mo.SetUpModePages(pages, 0x21, false);
    EXPECT_TRUE(pages.empty()) << "Unsupported vendor-specific page was returned";

    mo.SetBlockCount(0x12345678);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(1U, pages.size()) << "Unexpected number of mode pages";
    vector<byte> &page_32 = pages[32];
    EXPECT_EQ(12U, page_32.size());
    EXPECT_EQ(0, to_integer<int>(page_32[2])) << "Wrong format mode";
    EXPECT_EQ(0, to_integer<int>(page_32[3])) << "Wrong format type";
    EXPECT_EQ(0x12345678U, GetInt32(page_32, 4)) << "Wrong number of blocks";
    EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(0, GetInt16(page_32, 10));

    mo.SetSectorSizeInBytes(512);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(0, GetInt16(page_32, 10));

    mo.SetBlockCount(248826);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(1024, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(1, GetInt16(page_32, 10));

    mo.SetBlockCount(446325);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(1025, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(10, GetInt16(page_32, 10));

    mo.SetBlockCount(1041500);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(2250, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(18, GetInt16(page_32, 10));

    mo.SetSectorSizeInBytes(2048);
    mo.SetBlockCount(0x12345678);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(0, GetInt16(page_32, 10));

    mo.SetBlockCount(310352);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(2244, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(11, GetInt16(page_32, 10));

    mo.SetBlockCount(605846);
    mo.SetUpModePages(pages, 0x20, false);
    EXPECT_EQ(4437, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
    EXPECT_EQ(18, GetInt16(page_32, 10));

    // Changeable page
    mo.SetUpModePages(pages, 0x20, true);
    EXPECT_EQ(0, to_integer<int>(page_32[2]));
    EXPECT_EQ(0, to_integer<int>(page_32[3]));
    EXPECT_EQ(0U, GetInt32(page_32, 4));
    EXPECT_EQ(0, GetInt16(page_32, 8));
    EXPECT_EQ(0, GetInt16(page_32, 10));
}

TEST(OpticalMemoryTest, ModeSelect)
{
    MockOpticalMemory mo(0);
    vector<uint8_t> buf(32);

    mo.SetSectorSizeInBytes(2048);

    // PF (vendor-specific parameter format) must not fail but be ignored
    vector<int> cdb = CreateCdb(scsi_command::cmd_mode_select6, "10");

    // Page 3 (Format device page)
    buf[4] = 0x03;
    // Page length
    buf[5] = 0x16;
    EXPECT_THROW(mo.ModeSelect(cdb, buf, 28), scsi_exception)<< "Page 3 is not supported";

    // Page 1 (Read-write error recovery page)
    buf[4] = 0x01;
    // Page length
    buf[5] = 0x0a;
    EXPECT_NO_THROW(mo.ModeSelect(cdb, buf, 16));
    buf[4] = 0;
    buf[5] = 0;

    cdb = CreateCdb(scsi_command::cmd_mode_select10, "10");

    // Page 3 (Format device page)
    buf[8] = 0x04;
    // Page length
    buf[9] = 0x16;
    EXPECT_THROW(mo.ModeSelect(cdb, buf, 32), scsi_exception)<< "Page 3 is not supported";

    // Page 1 (Read-write error recovery page)
    buf[8] = 0x01;
    // Page length
    buf[9] = 0x0a;
    EXPECT_NO_THROW(mo.ModeSelect(cdb, buf, 20));
}
