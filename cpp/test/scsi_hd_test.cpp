//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

TEST(ScsiHdTest, SCHD_DeviceDefaults)
{
    DeviceFactory device_factory;

    auto device = device_factory.CreateDevice(UNDEFINED, 0, "test.hda");

    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCHD, device->GetType());
    EXPECT_TRUE(device->SupportsFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_TRUE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_FALSE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("QUANTUM", device->GetVendor()) << "Invalid default vendor for Apple drive";
    EXPECT_EQ("FIREBALL", device->GetProduct()) << "Invalid default vendor for Apple drive";
    EXPECT_EQ(TestShared::GetVersion(), device->GetRevision());

    device = device_factory.CreateDevice(UNDEFINED, 0, "test.hds");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCHD, device->GetType());
}

TEST(ScsiHdTest, SCRM_DeviceDefaults)
{
    TestShared::TestRemovableDrive(SCRM, "test.hdr", "SCSI HD (REM.)");
}

void ScsiHdTest_SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(5, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12, pages[1].size());
    EXPECT_EQ(24, pages[3].size());
    EXPECT_EQ(24, pages[4].size());
    EXPECT_EQ(12, pages[8].size());
    EXPECT_EQ(30, pages[48].size());
}

TEST(ScsiHdTest, Inquiry)
{
    TestShared::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_2, "SCSI2Pi                 ", 0x1f, false);

    TestShared::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_1_ccs, "SCSI2Pi                 ", 0x1f,
        false, "file.hd1");
}

TEST(ScsiHdTest, SupportsSaveParameters)
{
    MockScsiHd hd(0, false);

    EXPECT_TRUE(hd.SupportsSaveParameters());
}

TEST(ScsiHdTest, FinalizeSetup)
{
    MockScsiHd hd(0, false);

    hd.SetSectorSizeInBytes(1024);
    EXPECT_THROW(hd.FinalizeSetup(0), io_exception)<< "Device has 0 blocks";
}

TEST(ScsiHdTest, GetProductData)
{
    MockScsiHd hd_kb(0, false);
    MockScsiHd hd_mb(0, false);
    MockScsiHd hd_gb(0, false);

    const path filename = CreateTempFile(1);
    hd_kb.SetFilename(string(filename));
    hd_kb.SetSectorSizeInBytes(1024);
    hd_kb.SetBlockCount(1);
    hd_kb.FinalizeSetup(0);
    string s = hd_kb.GetProduct();
    EXPECT_NE(string::npos, s.find("1 KiB"));

    hd_mb.SetFilename(string(filename));
    hd_mb.SetSectorSizeInBytes(1024);
    hd_mb.SetBlockCount(1'048'576 / 1024);
    hd_mb.FinalizeSetup(0);
    s = hd_mb.GetProduct();
    EXPECT_NE(string::npos, s.find("1 MiB"));

    hd_gb.SetFilename(string(filename));
    hd_gb.SetSectorSizeInBytes(1024);
    hd_gb.SetBlockCount(10'737'418'240 / 1024);
    hd_gb.FinalizeSetup(0);
    s = hd_gb.GetProduct();
    EXPECT_NE(string::npos, s.find("10 GiB"));
    remove(filename);
}

TEST(ScsiHdTest, GetSectorSizes)
{
    MockScsiHd hd(0, false);

    const auto &sector_sizes = hd.GetSupportedSectorSizes();
    EXPECT_EQ(4, sector_sizes.size());

    EXPECT_TRUE(sector_sizes.contains(512));
    EXPECT_TRUE(sector_sizes.contains(1024));
    EXPECT_TRUE(sector_sizes.contains(2048));
    EXPECT_TRUE(sector_sizes.contains(4096));
}

TEST(ScsiHdTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockScsiHd hd(0, false);

    // Non changeable
    hd.SetUpModePages(pages, 0x3f, false);
    ScsiHdTest_SetUpModePages(pages);

    // Changeable
    pages.clear();
    hd.SetUpModePages(pages, 0x3f, true);
    ScsiHdTest_SetUpModePages(pages);
}

TEST(ScsiHdTest, ModeSelect)
{
    MockScsiHd hd( { 512 });
    vector<int> cmd(10);
    vector<uint8_t> buf(255);

    hd.SetSectorSizeInBytes(512);

    // PF
    cmd[1] = 0x10;
    // Page 3 (Device Format Page)
    buf[4] = 0x03;
    // 512 bytes per sector
    buf[16] = 0x02;
    EXPECT_NO_THROW(hd.ModeSelect(scsi_command::cmd_mode_select6, cmd, buf, 255))<< "MODE SELECT(6) is supported";
    buf[4] = 0;
    buf[16] = 0;

    // Page 3 (Device Format Page)
    buf[8] = 0x03;
    // 512 bytes per sector
    buf[20] = 0x02;
    EXPECT_NO_THROW(hd.ModeSelect(scsi_command::cmd_mode_select10, cmd, buf, 255))<< "MODE SELECT(10) is supported";
}
