//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "base/device_factory.h"

TEST(ScsiCdTest, DeviceDefaults)
{
    auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "test.iso");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCCD, device->GetType());
    EXPECT_TRUE(device->SupportsFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_FALSE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_TRUE(device->IsReadOnly());
    EXPECT_TRUE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_TRUE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("SCSI2Pi", device->GetVendor());
    EXPECT_EQ("SCSI CD-ROM", device->GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), device->GetRevision());
}

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

TEST(ScsiCdTest, Inquiry)
{
    TestShared::Inquiry(SCCD, device_type::cd_rom, scsi_level::scsi_2, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true);

    TestShared::Inquiry(SCCD, device_type::cd_rom, scsi_level::scsi_1_ccs, "SCSI2Pi SCSI CD-ROM     ", 0x1f, true,
        "file.is1");
}

TEST(ScsiCdTest, GetSectorSizes)
{
    MockScsiCd cd(0);

    const auto &sector_sizes = cd.GetSupportedSectorSizes();
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
    auto controller = make_shared<MockAbstractController>();
    auto cd = make_shared<MockScsiCd>(0);
    EXPECT_TRUE(cd->Init( { }));

    controller->AddDevice(cd);

    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::not_ready, asc::medium_not_present,
        "Drive is not ready");

    cd->SetReady(true);

    controller->SetCdbByte(6, 2);
    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid track number");

    controller->SetCdbByte(6, 1);
    TestShared::Dispatch(*cd, scsi_command::cmd_read_toc, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid track number");

    controller->SetCdbByte(6, 0);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(cd->Dispatch(scsi_command::cmd_read_toc));
}

TEST(ScsiCdTest, ReadData)
{
    MockScsiCd cd(0);

    EXPECT_THROW(cd.ReadData( {}), scsi_exception)<< "Drive is not ready";
}
