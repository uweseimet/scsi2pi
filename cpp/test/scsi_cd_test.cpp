//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_exceptions.h"

void ScsiCdTest_SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(7U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(12U, pages[7].size());
    EXPECT_EQ(12U, pages[8].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(8U, pages[13].size());
    EXPECT_EQ(24U, pages[48].size());
}

TEST(ScsiCdTest, DeviceDefaults)
{
    ScsiCd cd(0);

    EXPECT_EQ(SCCD, cd.GetType());
    EXPECT_TRUE(cd.SupportsFile());
    EXPECT_FALSE(cd.SupportsParams());
    EXPECT_FALSE(cd.IsProtectable());
    EXPECT_FALSE(cd.IsProtected());
    EXPECT_TRUE(cd.IsReadOnly());
    EXPECT_TRUE(cd.IsRemovable());
    EXPECT_FALSE(cd.IsRemoved());
    EXPECT_TRUE(cd.IsLockable());
    EXPECT_FALSE(cd.IsLocked());
    EXPECT_TRUE(cd.IsStoppable());
    EXPECT_FALSE(cd.IsStopped());

    EXPECT_EQ("SCSI2Pi", cd.GetVendor());
    EXPECT_EQ("SCSI CD-ROM", cd.GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), cd.GetRevision());
}

TEST(ScsiCdTest, Inquiry)
{
    TestShared::Inquiry(SCCD, device_type::cd_rom, scsi_level::scsi_2, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true);

    TestShared::Inquiry(SCCD, device_type::cd_rom, scsi_level::scsi_1_ccs, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true,
        "file.is1");
}

TEST(ScsiCdTest, GetBlockSizes)
{
    ScsiCd cd(0);

    const auto &sector_sizes = cd.GetSupportedBlockSizes();
    EXPECT_EQ(2U, sector_sizes.size());

    EXPECT_TRUE(sector_sizes.contains(512));
    EXPECT_TRUE(sector_sizes.contains(2048));
}

TEST(ScsiCdTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockScsiCd cd(0);

    // Non changeable
    cd.SetUpModePages(pages, 0x3f, false);
    ScsiCdTest_SetUpModePages(pages);

    // Changeable
    pages.clear();
    cd.SetUpModePages(pages, 0x3f, true);
    ScsiCdTest_SetUpModePages(pages);
}

TEST(ScsiCdTest, Open)
{
    MockScsiCd cd(0);

    EXPECT_THROW(cd.Open(), io_exception)<< "Missing filename";

    path filename = CreateTempFile(2047);
    cd.SetFilename(filename.string());
    EXPECT_THROW(cd.Open(), io_exception)<< "ISO CD-ROM image file size is too small";

    filename = CreateTempFile(2 * 2048);
    cd.SetFilename(filename.string());
    cd.Open();
    EXPECT_EQ(2U, cd.GetBlockCount());

    // Further testing requires filesystem access
}

TEST(ScsiCdTest, ReadToc)
{
    MockAbstractController controller;
    auto cd = make_shared<MockScsiCd>(0);
    EXPECT_TRUE(cd->Init( { }));

    controller.AddDevice(cd);

    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::not_ready, asc::medium_not_present,
        "Drive is not ready");

    cd->SetReady(true);

    controller.SetCdbByte(6, 2);
    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid track number");

    controller.SetCdbByte(6, 1);
    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid track number");

    controller.SetCdbByte(6, 0);
    EXPECT_CALL(controller, DataIn());
    EXPECT_NO_THROW(cd->Dispatch(scsi_command::cmd_read_toc));
}

TEST(ScsiCdTest, ReadData)
{
    ScsiCd cd(0);

    EXPECT_THROW(cd.ReadData( {}), scsi_exception)<< "Drive is not ready";
}
