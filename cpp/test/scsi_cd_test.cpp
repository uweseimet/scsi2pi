//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include <fstream>
#include "mocks.h"
#include "shared/shared_exceptions.h"

using namespace std;
using namespace filesystem;

TEST(ScsiCdTest, DeviceDefaults)
{
    DeviceFactory device_factory;

    auto device = device_factory.CreateDevice(UNDEFINED, 0, "test.iso");
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
    EXPECT_EQ(11U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(24U, pages[3].size());
    EXPECT_EQ(24U, pages[4].size());
    EXPECT_EQ(12U, pages[7].size());
    EXPECT_EQ(12U, pages[8].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(24U, pages[12].size());
    EXPECT_EQ(8U, pages[13].size());
    EXPECT_EQ(16U, pages[14].size());
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
    MockScsiCd cd_iso(0);
    MockScsiCd cd_cue(0);
    MockScsiCd cd_raw(0);
    MockScsiCd cd_physical(0);

    EXPECT_THROW(cd_iso.Open(), io_exception)<< "Missing filename";

    path filename = CreateTempFile(2047);
    cd_iso.SetFilename(string(filename));
    EXPECT_THROW(cd_iso.Open(), io_exception)<< "ISO CD-ROM image file size too small";
    remove(filename);

    filename = CreateTempFile(2 * 2048);
    cd_iso.SetFilename(string(filename));
    cd_iso.Open();
    EXPECT_EQ(2U, cd_iso.GetBlockCount());
    remove(filename);

    filename = CreateTempFile(0);
    ofstream out;
    out.open(filename);
    array<char, 4> cue = { 'F', 'I', 'L', 'E' };
    out.write(cue.data(), cue.size());
    out.close();
    resize_file(filename, 2 * 2048);
    cd_cue.SetFilename(string(filename));
    EXPECT_THROW(cd_cue.Open(), io_exception)<< "CUE CD-ROM files are not supported";

    filename = CreateTempFile(0);
    out.open(filename);
    array<char, 16> header;
    header.fill(0xff);
    header[0] = 0;
    header[11] = 0;
    out.write(header.data(), header.size());
    out.close();
    resize_file(filename, 2 * 2535);
    cd_raw.SetFilename(string(filename));
    EXPECT_THROW(cd_raw.Open(), io_exception)<< "Illegal raw ISO CD-ROM header";
    header[15] = 0x01;
    filename = CreateTempFile(0);
    out.open(filename);
    out.write(header.data(), header.size());
    out.close();
    cd_raw.SetFilename(string(filename));
    resize_file(filename, 2 * 2536);
    cd_raw.Open();
    EXPECT_EQ(2U, cd_raw.GetBlockCount());
    remove(filename);

    filename = CreateTempFile(2 * 2048);
    cd_physical.SetFilename("\\" + string(filename));
    // The respective code in SCSICD appears to be broken, see https://github.com/akuker/PISCSI/issues/919
    EXPECT_THROW(cd_physical.Open(), io_exception)<< "Invalid physical CD-ROM file";
    remove(filename);
}

TEST(ScsiCdTest, ReadToc)
{
    auto controller = make_shared<MockAbstractController>();
    auto cd = make_shared<MockScsiCd>(0);
    EXPECT_TRUE(cd->Init( { }));

    controller->AddDevice(cd);

    EXPECT_THAT([&]
        {
            cd->Dispatch(scsi_command::cmd_read_toc)
            ;
        },
        Throws<scsi_exception>(
            AllOf(Property(&scsi_exception::get_sense_key, sense_key::not_ready),
                Property(&scsi_exception::get_asc, asc::medium_not_present))));

    // Further testing requires filesystem access
}
