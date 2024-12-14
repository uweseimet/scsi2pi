//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "mocks.h"
#include "base/device_factory.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

static void ValidateModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(10U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(24U, pages[3].size());
    EXPECT_EQ(24U, pages[4].size());
    EXPECT_EQ(12U, pages[7].size());
    EXPECT_EQ(12U, pages[8].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(24U, pages[12].size());
    EXPECT_EQ(25U, pages[37].size());
    EXPECT_EQ(24U, pages[48].size());
}

static void ValidateFormatPage(const AbstractController &controller, int offset)
{
    const auto &buf = controller.GetBuffer();
    EXPECT_EQ(0x08, buf[offset + 3]) << "Wrong number of tracks in one zone";
    EXPECT_EQ(25, GetInt16(buf, offset + 10)) << "Wrong number of sectors per track";
    EXPECT_EQ(1024, GetInt16(buf, offset + 12)) << "Wrong number of bytes per sector";
    EXPECT_EQ(1, GetInt16(buf, offset + 14)) << "Wrong interleave";
    EXPECT_EQ(11, GetInt16(buf, offset + 16)) << "Wrong track skew factor";
    EXPECT_EQ(20, GetInt16(buf, offset + 18)) << "Wrong cylinder skew factor";
    EXPECT_FALSE(buf[offset + 20] & 0x20) << "Wrong removable flag";
    EXPECT_TRUE(buf[offset + 20] & 0x40) << "Wrong hard-sectored flag";
}

static void ValidateDrivePage(const AbstractController &controller, int offset)
{
    const auto &buf = controller.GetBuffer();
    EXPECT_EQ(0x17, buf[offset + 2]);
    EXPECT_EQ(0x4d3b, GetInt16(buf, offset + 3));
    EXPECT_EQ(8, buf[offset + 5]) << "Wrong number of heads";
    EXPECT_EQ(7200, GetInt16(buf, offset + 20)) << "Wrong medium rotation rate";
}

TEST(ScsiHdTest, SCHD_DeviceDefaults)
{
    auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "test.hda");

    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCHD, device->GetType());
    EXPECT_TRUE(device->SupportsImageFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_TRUE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_FALSE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("QUANTUM", device->GetVendor()) << "Invalid default vendor for Apple drive";
    EXPECT_EQ("FIREBALL", device->GetProduct()) << "Invalid default vendor for Apple drive";
    EXPECT_EQ(TestShared::GetVersion(), device->GetRevision());

    device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "test.hds");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCHD, device->GetType());
}

TEST(ScsiHdTest, SCRM_DeviceDefaults)
{
    TestShared::TestRemovableDrive(SCRM, "test.hdr", "SCSI HD (REM.)");
}

TEST(ScsiHdTest, Inquiry)
{
    TestShared::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_2, "SCSI2Pi                 ", 0x1f, false);

    TestShared::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_1_ccs, "SCSI2Pi                 ", 0x1f,
        false, "file.hd1");
}

TEST(ScsiHdTest, FinalizeSetup)
{
    MockScsiHd hd(0, false);

    hd.SetBlockSize(1024);
    EXPECT_THROW(hd.FinalizeSetup(), io_exception)<< "Device has 0 blocks";
}

TEST(ScsiHdTest, GetProductData)
{
    MockScsiHd hd_kb(0, false);
    MockScsiHd hd_mb(0, false);
    MockScsiHd hd_gb(0, false);

    const path filename = CreateTempFile(1);
    hd_kb.SetFilename(filename.string());
    hd_kb.SetBlockSize(1024);
    hd_kb.SetBlockCount(1);
    hd_kb.FinalizeSetup();
    string s = hd_kb.GetProduct();
    EXPECT_NE(string::npos, s.find("1 KiB"));

    hd_mb.SetFilename(filename.string());
    hd_mb.SetBlockSize(1024);
    hd_mb.SetBlockCount(1'048'576 / 1024);
    hd_mb.FinalizeSetup();
    s = hd_mb.GetProduct();
    EXPECT_NE(string::npos, s.find("1 MiB"));

    hd_gb.SetFilename(filename.string());
    hd_gb.SetBlockSize(1024);
    hd_gb.SetBlockCount(10'737'418'240 / 1024);
    hd_gb.FinalizeSetup();
    s = hd_gb.GetProduct();
    EXPECT_NE(string::npos, s.find("10 GiB"));
}

TEST(ScsiHdTest, GetBlockSizes)
{
    MockScsiHd hd(0, false);

    const auto &sizes = hd.GetSupportedBlockSizes();
    EXPECT_EQ(4U, sizes.size());

    EXPECT_TRUE(sizes.contains(512));
    EXPECT_TRUE(sizes.contains(1024));
    EXPECT_TRUE(sizes.contains(2048));
    EXPECT_TRUE(sizes.contains(4096));
}

TEST(ScsiHdTest, ConfiguredBlockSize)
{
    MockScsiHd hd(0, false);

    EXPECT_TRUE(hd.SetConfiguredBlockSize(512));
    EXPECT_EQ(512U, hd.GetConfiguredBlockSize());

    EXPECT_TRUE(hd.SetConfiguredBlockSize(4));
    EXPECT_EQ(4U, hd.GetConfiguredBlockSize());

    EXPECT_FALSE(hd.SetConfiguredBlockSize(1234));
    EXPECT_EQ(4U, hd.GetConfiguredBlockSize());
}

TEST(ScsiHdTest, ValidateBlockSize)
{
    MockScsiHd hd(0, false);
    EXPECT_FALSE(hd.ValidateBlockSize(0));
    EXPECT_TRUE(hd.ValidateBlockSize(4));
    EXPECT_FALSE(hd.ValidateBlockSize(7));
    EXPECT_TRUE(hd.ValidateBlockSize(512));
    EXPECT_TRUE(hd.ValidateBlockSize(131072));

    MockScsiHd rm(0, true);
    EXPECT_FALSE(rm.ValidateBlockSize(0));
    EXPECT_FALSE(rm.ValidateBlockSize(4));
    EXPECT_FALSE(rm.ValidateBlockSize(7));
    EXPECT_TRUE(hd.ValidateBlockSize(512));
    EXPECT_FALSE(rm.ValidateBlockSize(131072));
}

TEST(ScsiHdTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockScsiHd hd(0, false);

    // Non changeable
    hd.SetUpModePages(pages, 0x3f, false);
    ValidateModePages(pages);

    // Changeable
    pages.clear();
    hd.SetUpModePages(pages, 0x3f, true);
    ValidateModePages(pages);
}

TEST(ScsiHdTest, ModeSense6)
{
    NiceMock<MockAbstractController> controller(0);
    auto hd = make_shared<MockScsiHd>(0, false);
    EXPECT_TRUE(hd->Init());
    EXPECT_TRUE(controller.AddDevice(hd));

    // Drive must be ready in order to return all data
    hd->SetReady(true);

    // ALLOCATION LENGTH
    controller.SetCdbByte(4, 255);

    // Return short block descriptor
    controller.SetCdbByte(1, 0x00);

    // Format page
    controller.SetCdbByte(2, 0x03);
    // ALLOCATION LENGTH
    controller.SetCdbByte(4, 255);
    hd->SetBlockSize(1024);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::mode_sense_6));
    ValidateFormatPage(controller, 12);

    // Rigid disk drive page
    controller.SetCdbByte(2, 0x04);
    // ALLOCATION LENGTH
    controller.SetCdbByte(4, 255);
    hd->SetBlockCount(0x12345678);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::mode_sense_6));
    ValidateDrivePage(controller, 12);
}

TEST(ScsiHdTest, ModeSense10)
{
    NiceMock<MockAbstractController> controller(0);
    auto hd = make_shared<MockScsiHd>(0, false);
    EXPECT_TRUE(hd->Init());
    EXPECT_TRUE(controller.AddDevice(hd));

    // Drive must be ready in order to return all data
    hd->SetReady(true);

    // ALLOCATION LENGTH
    controller.SetCdbByte(8, 255);

    // Return short block descriptor
    controller.SetCdbByte(1, 0x00);

    // Format page
    controller.SetCdbByte(2, 0x03);
    // ALLOCATION LENGTH
    controller.SetCdbByte(8, 255);
    hd->SetBlockSize(1024);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::mode_sense_10));
    ValidateFormatPage(controller, 16);

    // Rigid disk drive page
    controller.SetCdbByte(2, 0x04);
    // ALLOCATION LENGTH
    controller.SetCdbByte(8, 255);
    hd->SetBlockCount(0x12345678);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::mode_sense_10));
    ValidateDrivePage(controller, 16);
}

TEST(ScsiHdTest, ModeSelect)
{
    MockScsiHd hd( { 512 });
    vector<uint8_t> buf(32);

    hd.SetBlockSize(512);

    // PF (vendor-specific parameter format) must not fail but be ignored
    auto cdb = CreateCdb(scsi_command::mode_select_6, "10");

    // Page 0
    EXPECT_THROW(hd.ModeSelect(cdb, buf, 16, 0), scsi_exception);

    // Page 1 (Read-write error recovery page)
    buf[4] = 0x01;
    // Page length
    buf[5] = 0x0a;
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 16, 0));
    buf[4] = 0;
    buf[5] = 0;

    cdb = CreateCdb(scsi_command::mode_select_10, "10");

    // Page 1 (Read-write error recovery page)
    buf[8] = 0x01;
    // Page length
    buf[9] = 0x0a;
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 20, 0));
}

TEST(ScsiHdTest, ModeSelect6_Single)
{
    vector<uint8_t> buf(28);
    MockScsiHd hd( { 512, 1024, 2048 });
    hd.SetBlockSize(1024);

    // PF (standard parameter format)
    const auto &cdb = CreateCdb(scsi_command::mode_select_6, "10");

    // A length of 0 is valid, the page data are optional
    hd.SetBlockSize(512);
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 0, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 0
    buf[4] = 0x00;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size(), 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Unsupported page 0 was not rejected";

    // Page 1 (Read-write error recovery page)
    buf[4] = 0x01;
    // Page length
    buf[5] = 0x0a;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, 12, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Not enough command parameters";
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 16, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 7 (Verify error recovery page)
    buf[4] = 0x07;
    // Page length
    buf[5] = 0x0a;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, 2, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 16, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 3 (Format device page)
    buf[4] = 0x03;
    // Page length
    buf[5] = 0x16;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size(), 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Requested sector size does not match current sector size";

    // Match the requested to the current sector size
    buf[16] = 0x08;
    hd.SetBlockSize(2048);
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size() - 10, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Not enough command parameters";

    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());
}

TEST(ScsiHdTest, ModeSelect6_Multiple)
{
    const string &format_device_1 =
        R"(00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_2 =
        R"(00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_3 =
        R"(00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:04:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_4 =
        R"(00:00:00:00
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
03:16:00:08:00:00:00:00:00:00:00:19:04:00:00:01:00:0b:00:14:00:00:00:00
)";

    MockScsiHd hd( { 512, 1024, 2048 });
    hd.SetBlockSize(2048);

    // Select sector size of 2048 bytes, which is the current size, once
    auto buf = CreateParameters(format_device_1);
    auto cdb = CreateCdb(scsi_command::mode_select_6, fmt::format("10:00:00:{:02x}", buf.size()));
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes, which is the current size, twice
    buf = CreateParameters(format_device_2);
    cdb = CreateCdb(scsi_command::mode_select_6, fmt::format("10:00:00:{:02x}", buf.size()));
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes, which is the current size, twice, then try to select a size of 1024 bytes
    buf = CreateParameters(format_device_3);
    cdb = CreateCdb(scsi_command::mode_select_6, fmt::format("10:00:00:{:02x}", buf.size()));
    EXPECT_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0), scsi_exception);
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes after a sequence of other mode pages
    buf = CreateParameters(format_device_4);
    cdb = CreateCdb(scsi_command::mode_select_6, fmt::format("10:00:00:{:02x}", buf.size()));
    EXPECT_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0), scsi_exception);
    EXPECT_EQ(2048U, hd.GetBlockSize());
}

TEST(ScsiHdTest, ModeSelect10_Single)
{
    vector<uint8_t> buf(32);
    MockScsiHd hd( { 512, 1024, 2048 });
    hd.SetBlockSize(1024);

    // PF (standard parameter format)
    const auto &cdb = CreateCdb(scsi_command::mode_select_10, "10");

    // A length of 0 is valid, the page data are optional
    hd.SetBlockSize(512);
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 0, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 0
    buf[8] = 0x00;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size(), 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Unsupported page 0 was not rejected";

    // Page 1 (Read-write error recovery page)
    buf[8] = 0x01;
    // Page length
    buf[9] = 0x0a;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, 16, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Not enough command parameters";
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 20, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 7 (Verify error recovery page)
    buf[8] = 0x07;
    // Page length
    buf[9] = 0x0a;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, 2, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, 20, 0));
    EXPECT_EQ(512U, hd.GetBlockSize());

    // Page 3 (Format device page)
    buf[8] = 0x03;
    // Page length
    buf[9] = 0x16;
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size(), 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Requested sector size does not match current sector size";

    // Match the requested to the current sector size
    buf[20] = 0x08;
    hd.SetBlockSize(2048);
    EXPECT_THAT([&] {hd.ModeSelect(cdb, buf, buf.size() - 10, 0);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Not enough command parameters";

    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());
}

TEST(ScsiHdTest, ModeSelect10_Multiple)
{
    const string &format_device_1 =
        R"(00:00:00:00:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_2 =
        R"(00:00:00:00:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_3 =
        R"(00:00:00:00:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:08:00:00:01:00:0b:00:14:00:00:00:00
03:16:00:08:00:00:00:00:00:00:00:19:04:00:00:01:00:0b:00:14:00:00:00:00
)";

    const string &format_device_4 =
        R"(00:00:00:00:00:00:00:00
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
08:0a:01:02:03:04:05:06:07:08:09:0a
03:16:00:08:00:00:00:00:00:00:00:19:04:00:00:01:00:0b:00:14:00:00:00:00
)";

    MockScsiHd hd( { 512, 1024, 2048 });
    hd.SetBlockSize(2048);

    // Select sector size of 2048 bytes, which is the current size, once
    auto buf = CreateParameters(format_device_1);
    auto cdb = CreateCdb(scsi_command::mode_select_10, fmt::format("10:00:00:00:00:00:00:{:02x}", buf.size()));
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes, which is the current size, twice
    buf = CreateParameters(format_device_2);
    cdb = CreateCdb(scsi_command::mode_select_10, fmt::format("10:00:00:00:00:00:00:{:02x}", buf.size()));
    EXPECT_NO_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0));
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes, which is the current size, twice, then try to select a size of 1024 bytes
    buf = CreateParameters(format_device_3);
    cdb = CreateCdb(scsi_command::mode_select_10, fmt::format("10:00:00:00:00:00:00:{:02x}", buf.size()));
    EXPECT_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0), scsi_exception);
    EXPECT_EQ(2048U, hd.GetBlockSize());

    // Select sector size of 2048 bytes after a sequence of other mode pages
    buf = CreateParameters(format_device_4);
    cdb = CreateCdb(scsi_command::mode_select_10, fmt::format("10:00:00:00:00:00:00:{:02x}", buf.size()));
    EXPECT_THROW(hd.ModeSelect(cdb, buf, buf.size(), 0), scsi_exception);
    EXPECT_EQ(2048U, hd.GetBlockSize());
}

TEST(ScsiHdTest, Open)
{
    MockScsiHd hd(0, false);

    EXPECT_THROW(hd.Open(), io_exception)<< "Missing filename";

    const path &filename = CreateTempFile(2048);
    hd.SetFilename(filename.string());
    hd.Open();
    EXPECT_EQ(4U, hd.GetBlockCount());
}

