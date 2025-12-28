//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_exceptions.h"

static void ValidateModePages(map<int, vector<byte>> &pages)
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
    ScsiCd cd(0, false);

    EXPECT_EQ(SCCD, cd.GetType());
    EXPECT_TRUE(cd.SupportsImageFile());
    EXPECT_FALSE(cd.SupportsParams());
    EXPECT_FALSE(cd.IsProtectable());
    EXPECT_FALSE(cd.IsProtected());
    EXPECT_TRUE(cd.IsReadOnly());
    EXPECT_TRUE(cd.IsRemovable());
    EXPECT_FALSE(cd.IsRemoved());
    EXPECT_FALSE(cd.IsLocked());
    EXPECT_TRUE(cd.IsStoppable());
    EXPECT_FALSE(cd.IsStopped());

    const auto& [vendor, product, revision] = cd.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("SCSI CD-ROM", product);
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST(ScsiCdTest, Inquiry)
{
    TestShared::Inquiry(SCCD, DeviceType::CD_DVD, ScsiLevel::SCSI_2, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true);

    TestShared::Inquiry(SCCD, DeviceType::CD_DVD, ScsiLevel::SCSI_1_CCS, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true,
        "file.is1");
}

TEST(ScsiCdTest, GetBlockSizes)
{
    ScsiCd cd(0, false);

    const auto &sizes = cd.GetSupportedBlockSizes();
    EXPECT_EQ(2U, sizes.size());

    EXPECT_TRUE(sizes.contains(512));
    EXPECT_TRUE(sizes.contains(2048));
}

TEST(ScsiCdTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockScsiCd cd(0);

    // Non changeable
    cd.SetUpModePages(pages, 0x3f, false);
    ValidateModePages(pages);

    // Changeable
    pages.clear();
    cd.SetUpModePages(pages, 0x3f, true);
    ValidateModePages(pages);
}

TEST(ScsiCdTest, Open)
{
    MockScsiCd cd(0);

    EXPECT_THROW(cd.Open(), IoException)<< "Missing filename";

    cd.SetFilename(CreateTempFile(2047).string());
    EXPECT_THROW(cd.Open(), IoException)<< "ISO CD-ROM image file size is too small";

    cd.SetFilename(CreateTempFile(2 * 2048).string());
    cd.Open();
    EXPECT_EQ(2U, cd.GetBlockCount());
}

TEST(ScsiCdTest, ReadToc)
{
    MockAbstractController controller;
    auto cd = make_shared<MockScsiCd>(0);
    EXPECT_EQ("", cd->Init());

    controller.AddDevice(cd);

    Dispatch(cd, ScsiCommand::READ_TOC, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT, "Drive is not ready");

    cd->SetBlockSize(2048);
    cd->SetBlockCount(1);
    cd->SetFilename(CreateTempFile(2048).string());
    cd->ValidateFile();

    controller.SetCdbByte(6, 1);
    Dispatch(cd, ScsiCommand::READ_TOC, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB, "Invalid track number");

    controller.SetCdbByte(6, 0);
    EXPECT_CALL(controller, DataIn);
    EXPECT_NO_THROW(Dispatch(cd, ScsiCommand::READ_TOC));
    controller.SetCdbByte(1, 0x02);
    EXPECT_CALL(controller, DataIn);
    EXPECT_NO_THROW(Dispatch(cd, ScsiCommand::READ_TOC));
}

TEST(ScsiCdTest, ReadData)
{
    ScsiCd cd(0, false);

    EXPECT_THROW(cd.ReadData( {}), ScsiException)<< "Drive is not ready";
}

TEST(ScsiCdTest, ModeSelect)
{
    ScsiCd cd(0, false);

    EXPECT_NO_THROW(cd.ModeSelect( { }, { }, 0));
}
