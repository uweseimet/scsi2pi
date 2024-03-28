//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "devices/disk.h"

using namespace memory_util;

pair<shared_ptr<MockAbstractController>, shared_ptr<MockDisk>> CreateDisk()
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(0);
    auto disk = make_shared<MockDisk>();
    EXPECT_TRUE(disk->Init( { }));
    EXPECT_TRUE(controller->AddDevice(disk));

    return {controller, disk};
}

TEST(DiskTest, Dispatch)
{
    auto [controller, disk] = CreateDisk();

    disk->SetRemovable(true);
    disk->SetMediumChanged(false);
    disk->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status::good, controller->GetStatus());

    disk->SetMediumChanged(true);
    EXPECT_THROW(disk->Dispatch(scsi_command::cmd_test_unit_ready), scsi_exception);
    EXPECT_FALSE(disk->IsMediumChanged());
}

TEST(DiskTest, Rezero)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_rezero, sense_key::not_ready, asc::medium_not_present,
        "REZERO must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_rezero));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, FormatUnit)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_format_unit, sense_key::not_ready, asc::medium_not_present,
        "FORMAT UNIT must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_format_unit));
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCdbByte(1, 0x10);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_format_unit, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(DiskTest, ReassignBlocks)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_reassign_blocks, sense_key::not_ready, asc::medium_not_present,
        "REASSIGN must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_reassign_blocks));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, Seek6)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_seek6, sense_key::illegal_request, asc::lba_out_of_range,
        "SEEK(6) must fail for a medium with 0 sectors");

    disk->SetBlockCount(1);
    // Sector count
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_seek6, sense_key::not_ready, asc::medium_not_present,
        "SEEK(6) must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_seek6));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, Seek10)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_seek10, sense_key::illegal_request, asc::lba_out_of_range,
        "SEEK(10) must fail for a medium with 0 sectors");

    disk->SetBlockCount(1);
    // Sector count
    controller->SetCdbByte(5, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_seek10, sense_key::not_ready, asc::medium_not_present,
        "SEEK(10) must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_seek10));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, ReadCapacity10)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity10, sense_key::not_ready, asc::medium_not_present,
        "READ CAPACITY(10) must fail because drive is not ready");

    disk->SetReady(true);

    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity10, sense_key::illegal_request, asc::medium_not_present,
        "READ CAPACITY(10) must fail because the medium has no capacity");

    disk->SetBlockCount(0x12345678);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_capacity10));
    auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x1234, GetInt16(buf, 0));
    EXPECT_EQ(0x5677, GetInt16(buf, 2));

    disk->SetBlockCount(0x1234567887654321);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_capacity10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0xffff, GetInt16(buf, 0));
    EXPECT_EQ(0xffff, GetInt16(buf, 2));
}

TEST(DiskTest, ReadCapacity16)
{
    auto [controller, disk] = CreateDisk();

    controller->SetCdbByte(1, 0x00);

    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity16_read_long16, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Neither READ CAPACITY(16) nor READ LONG(16)");

    // READ CAPACITY(16), not READ LONG(16)
    controller->SetCdbByte(1, 0x10);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity16_read_long16, sense_key::not_ready,
        asc::medium_not_present, "READ CAPACITY(16) must fail because drive is not ready");

    disk->SetReady(true);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity16_read_long16, sense_key::illegal_request,
        asc::medium_not_present, "READ CAPACITY(16) must fail because the medium has no capacity");

    disk->SetBlockCount(0x1234567887654321);
    disk->SetSectorSizeInBytes(1024);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_capacity16_read_long16));
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x1234, GetInt16(buf, 0));
    EXPECT_EQ(0x5678, GetInt16(buf, 2));
    EXPECT_EQ(0x8765, GetInt16(buf, 4));
    EXPECT_EQ(0x4320, GetInt16(buf, 6));
    EXPECT_EQ(0x0000, GetInt16(buf, 8));
    EXPECT_EQ(0x0400, GetInt16(buf, 10));
}

TEST(DiskTest, Read6)
{
    auto [_, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_read6, sense_key::illegal_request,
        asc::lba_out_of_range, "READ(6) must fail for a medium with 0 blocks");

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Read10)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_read10, sense_key::illegal_request,
        asc::lba_out_of_range, "READ(10) must fail for a medium with 0 blocks");

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read10));
    EXPECT_EQ(status::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Read16)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_read16, sense_key::illegal_request,
        asc::lba_out_of_range, "READ(16) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read16));
    EXPECT_EQ(status::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Write6)
{
    auto [_, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_write6, sense_key::illegal_request,
        asc::lba_out_of_range, "WRITE(6) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    disk->SetReady(true);
    disk->SetProtectable(true);
    disk->SetProtected(true);
    TestShared::Dispatch(*disk, scsi_command::cmd_write6, sense_key::data_protect,
        asc::write_protected, "WRITE(6) must fail because drive is write-protected");

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Write10)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_write10, sense_key::illegal_request,
        asc::lba_out_of_range, "WRITE(10) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_write10));
    EXPECT_EQ(status::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Write16)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_write16, sense_key::illegal_request,
        asc::lba_out_of_range, "WRITE(16) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_write16));
    EXPECT_EQ(status::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());
}

TEST(DiskTest, Verify10)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_verify10, sense_key::illegal_request,
        asc::lba_out_of_range, "VERIFY(10) must fail for a medium with 0 blocks");

    disk->SetReady(true);
    // Verify 0 sectors
    disk->SetBlockCount(1);
    EXPECT_CALL(*disk, FlushCache());
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_verify10));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, Verify16)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_verify16, sense_key::illegal_request,
        asc::lba_out_of_range, "VERIFY(16) must fail for a medium with 0 blocks");

    disk->SetReady(true);
    // Verify 0 sectors
    disk->SetBlockCount(1);
    EXPECT_CALL(*disk, FlushCache());
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_verify16));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, ReadLong10)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_long10));
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCdbByte(1, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_long10, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "READ LONG(10) must fail because the RelAdr bit is set");
    controller->SetCdbByte(1, 0);

    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_long10, sense_key::illegal_request,
        asc::lba_out_of_range, "READ LONG(10) must fail because the capacity is exceeded");
    controller->SetCdbByte(2, 0);

    controller->SetCdbByte(7, 255);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_long10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "READ LONG(10) must fail because it only supports a limited transfer length");
}

TEST(DiskTest, ReadLong16)
{
    auto [controller, disk] = CreateDisk();

    // READ LONG(16), not READ CAPACITY(16)
    controller->SetCdbByte(1, 0x11);

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_capacity16_read_long16));
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity16_read_long16, sense_key::illegal_request,
        asc::lba_out_of_range, "READ LONG(16) must fail because the capacity is exceeded");
    controller->SetCdbByte(2, 0);

    controller->SetCdbByte(12, 55);
    TestShared::Dispatch(*disk, scsi_command::cmd_read_capacity16_read_long16, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "READ LONG(16) must fail because it only supports a limited transfer length");
}

TEST(DiskTest, WriteLong10)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_write_long10));
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCdbByte(1, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_write_long10, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "WRITE LONG(10) must fail because the RelAdr bit is set");
    controller->SetCdbByte(1, 0);

    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_write_long10, sense_key::illegal_request,
        asc::lba_out_of_range, "WRITE LONG(10) must fail because the capacity is exceeded");
    controller->SetCdbByte(2, 0);

    controller->SetCdbByte(7, 255);
    TestShared::Dispatch(*disk, scsi_command::cmd_write_long10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "WRITE LONG(10) must fail because it only supports a limited transfer length");
}

TEST(DiskTest, WriteLong16)
{
    auto [controller, disk] = CreateDisk();

    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*disk, scsi_command::cmd_write_long16, sense_key::illegal_request,
        asc::lba_out_of_range, "WRITE LONG(16) must fail because the capacity is exceeded");
    controller->SetCdbByte(2, 0);

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_write_long16));
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCdbByte(12, 255);
    TestShared::Dispatch(*disk, scsi_command::cmd_write_long16, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "WRITE LONG(16) must fail because it only supports a limited transfer length");
}

TEST(DiskTest, StartStopUnit)
{
    auto [controller, disk] = CreateDisk();

    disk->SetRemovable(true);

    // Stop/Unload
    disk->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_CALL(*disk, FlushCache);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_TRUE(disk->IsStopped());

    // Stop/Load
    controller->SetCdbByte(4, 0x02);
    disk->SetReady(true);
    disk->SetLocked(false);
    EXPECT_CALL(*controller, Status);
    EXPECT_CALL(*disk, FlushCache);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());

    disk->SetReady(false);
    EXPECT_CALL(*disk, FlushCache).Times(0);
    TestShared::Dispatch(*disk, scsi_command::cmd_start_stop, sense_key::illegal_request,
        asc::load_or_eject_failed, "START/STOP must fail because drive is not ready");

    disk->SetReady(true);
    disk->SetLocked(true);
    EXPECT_CALL(*disk, FlushCache).Times(0);
    TestShared::Dispatch(*disk, scsi_command::cmd_start_stop, sense_key::illegal_request,
        asc::load_or_eject_failed, "LOAD/EJECT must fail because drive is locked");

    // Start/Unload
    controller->SetCdbByte(4, 0x01);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_FALSE(disk->IsStopped());

    // Start/Load
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());

    // Start/Load with previous medium
    controller->SetCdbByte(4, 0x02);
    disk->SetLocked(false);
    disk->SetFilename("filename");
    EXPECT_CALL(*controller, Status);
    EXPECT_CALL(*disk, FlushCache);
    // Eject existing medium
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_TRUE(disk->GetFilename().empty());
    // Re-load medium
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_EQ("filename", disk->GetFilename());
}

TEST(DiskTest, PreventAllowMediumRemoval)
{
    auto [controller, disk] = CreateDisk();

    TestShared::Dispatch(*disk, scsi_command::cmd_prevent_allow_medium_removal, sense_key::not_ready,
        asc::medium_not_present, "PREVENT/ALLOW MEDIUM REMOVAL must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_prevent_allow_medium_removal));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_FALSE(disk->IsLocked());

    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_prevent_allow_medium_removal));
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_TRUE(disk->IsLocked());
}

TEST(DiskTest, Eject)
{
    MockDisk disk;

    disk.SetReady(false);
    disk.SetRemovable(false);
    disk.SetLocked(false);
    EXPECT_CALL(disk, FlushCache).Times(0);
    EXPECT_FALSE(disk.Eject(false));

    disk.SetRemovable(true);
    EXPECT_CALL(disk, FlushCache).Times(0);
    EXPECT_FALSE(disk.Eject(false));

    disk.SetReady(true);
    disk.SetLocked(true);
    EXPECT_CALL(disk, FlushCache).Times(0);
    EXPECT_FALSE(disk.Eject(false));

    disk.SetReady(true);
    disk.SetLocked(false);
    EXPECT_CALL(disk, FlushCache);
    EXPECT_TRUE(disk.Eject(false));

    disk.SetReady(true);
    EXPECT_CALL(disk, FlushCache);
    EXPECT_TRUE(disk.Eject(true));
}

void ValidateCachePage(AbstractController &controller, int offset)
{
    const auto &buf = controller.GetBuffer();
    EXPECT_EQ(0xffff, GetInt16(buf, offset + 4)) << "Wrong pre-fetch transfer length";
    EXPECT_EQ(0xffff, GetInt16(buf, offset + 8)) << "Wrong maximum pre-fetch";
    EXPECT_EQ(0xffff, GetInt16(buf, offset + 10)) << "Wrong maximum pre-fetch ceiling";
}

TEST(DiskTest, AddAppleVendorPage)
{
    MockDisk disk;

    map<int, vector<byte>> pages;
    vector<byte> vendor_page(30);
    pages[48] = vendor_page;

    disk.AddAppleVendorPage(pages, true);
    vendor_page = pages[48];
    EXPECT_EQ(byte { 0 }, vendor_page[2]);

    disk.AddAppleVendorPage(pages, false);
    vendor_page = pages[48];
    EXPECT_STREQ("APPLE COMPUTER, INC   ", (const char* )&vendor_page[2]);
}

TEST(DiskTest, ModeSense6)
{
    auto [controller, disk] = CreateDisk();

    // Drive must be ready in order to return all data
    disk->SetReady(true);

    controller->SetCdbByte(2, 0x3f);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense6));
    EXPECT_EQ(0x08, controller->GetBuffer()[3]) << "Wrong block descriptor length";

    // No block descriptor
    controller->SetCdbByte(1, 0x08);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense6));
    EXPECT_EQ(0x00, controller->GetBuffer()[2]) << "Wrong device-specific parameter";

    disk->SetReadOnly(false);
    disk->SetProtectable(true);
    disk->SetProtected(true);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense6));
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x80, buf[2]) << "Wrong device-specific parameter";

    // Return short block descriptor
    controller->SetCdbByte(1, 0x00);

    // Caching page
    controller->SetCdbByte(2, 0x08);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense6));
    ValidateCachePage(*controller, 12);
}

TEST(DiskTest, ModeSense10)
{
    auto [controller, disk] = CreateDisk();

    // Drive must be ready in order to return all data
    disk->SetReady(true);

    controller->SetCdbByte(2, 0x3f);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    EXPECT_EQ(0x08, controller->GetBuffer()[7]) << "Wrong block descriptor length";

    // No block descriptor
    controller->SetCdbByte(1, 0x08);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x00, controller->GetBuffer()[3]) << "Wrong device-specific parameter";

    disk->SetReadOnly(false);
    disk->SetProtectable(true);
    disk->SetProtected(true);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0x80, buf[3]) << "Wrong device-specific parameter";

    // Return short block descriptor
    controller->SetCdbByte(1, 0x00);
    disk->SetBlockCount(0x1234);
    disk->SetSectorSizeInBytes(1024);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0x00, buf[4]) << "Wrong LONGLBA field";
    EXPECT_EQ(0x08, buf[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00, GetInt16(buf, 8));
    EXPECT_EQ(0x1234, GetInt16(buf, 10));
    EXPECT_EQ(0x00, GetInt16(buf, 12));
    EXPECT_EQ(1024, GetInt16(buf, 14));

    // Return long block descriptor
    controller->SetCdbByte(1, 0x10);
    disk->SetBlockCount((uint64_t)0xffffffff + 1);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0x01, buf[4]) << "Wrong LONGLBA field";
    EXPECT_EQ(0x10, buf[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00, GetInt16(buf, 8));
    EXPECT_EQ(0x01, GetInt16(buf, 10));
    EXPECT_EQ(0x00, GetInt16(buf, 12));
    EXPECT_EQ(0x00, GetInt16(buf, 14));
    EXPECT_EQ(0x00, GetInt16(buf, 20));
    EXPECT_EQ(1024, GetInt16(buf, 22));

    // Return short block descriptor
    controller->SetCdbByte(1, 0x00);

    // Caching page
    controller->SetCdbByte(2, 0x08);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_mode_sense10));
    ValidateCachePage(*controller, 16);
}

TEST(DiskTest, EvaluateBlockDescriptors)
{
    vector<uint8_t> buf(14);
    int sector_size = 512;
    MockDisk disk;

    EXPECT_THAT([&] {disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, {}, sector_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
        << "Parameter list is too short";
    EXPECT_EQ(512, sector_size);

    EXPECT_THAT([&] {disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, {}, sector_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
        << "Parameter list is too short";
    EXPECT_EQ(512, sector_size);

    EXPECT_THAT([&] {disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, {}, sector_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
        << "Parameter list is too short";
    EXPECT_EQ(512, sector_size);

    EXPECT_THAT([&] {disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, {}, sector_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
        << "Parameter list is too short";
    EXPECT_EQ(512, sector_size);

    buf = CreateParameters("00:00:00:04:00:00:00:00:00:00:08:00");
    EXPECT_NO_THROW(disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, buf, sector_size));
    EXPECT_EQ(2048, sector_size);

    buf = CreateParameters("00:00:00:04:00:00:00:00:00:00:08:04");
    EXPECT_NO_THROW(disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, buf, sector_size));
    EXPECT_EQ(2052, sector_size);

    buf = CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:04:00");
    EXPECT_NO_THROW(disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, buf, sector_size));
    EXPECT_EQ(1024, sector_size);

    buf = CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:03:fc");
    EXPECT_NO_THROW(disk.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, buf, sector_size));
    EXPECT_EQ(1020, sector_size);
}

TEST(DiskTest, VerifySectorSizeChange)
{
    MockDisk disk;
    disk.SetSectorSizeInBytes(512);

    EXPECT_EQ(512, disk.VerifySectorSizeChange(512, false));

    EXPECT_EQ(1024, disk.VerifySectorSizeChange(1024, true));

    EXPECT_THAT([&] {disk.VerifySectorSizeChange(2048, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Parameter list is invalid";

    EXPECT_THAT([&] {disk.VerifySectorSizeChange(0, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Parameter list is invalid";
    EXPECT_THAT([&] {disk.VerifySectorSizeChange(513, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Parameter list is invalid";
    EXPECT_THAT([&] {disk.VerifySectorSizeChange(0, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Parameter list is invalid";
    EXPECT_THAT([&] {disk.VerifySectorSizeChange(513, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Parameter list is invalid";
}

TEST(DiskTest, ReadData)
{
    MockDisk disk;

    EXPECT_THAT([&] {disk.ReadData( {});}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::not_ready),
                Property(&scsi_exception::get_asc, asc::medium_not_present)))) << "Disk is not ready";
}

TEST(DiskTest, WriteData)
{
    MockDisk disk;

    EXPECT_THAT([&] {disk.WriteData( {}, scsi_command::cmd_write6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::not_ready),
                Property(&scsi_exception::get_asc, asc::medium_not_present)))) << "Disk is not ready";
}

TEST(DiskTest, SynchronizeCache)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_synchronize_cache10));
    EXPECT_EQ(status::good, controller->GetStatus());

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_synchronize_cache16));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, ReadDefectData)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(disk->Dispatch(scsi_command::cmd_read_defect_data10));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(DiskTest, ConfiguredSectorSize)
{
    MockScsiHd disk(0, false);

    EXPECT_TRUE(disk.SetConfiguredSectorSize(512));
    EXPECT_EQ(512U, disk.GetConfiguredSectorSize());

    EXPECT_FALSE(disk.SetConfiguredSectorSize(1234));
    EXPECT_EQ(512U, disk.GetConfiguredSectorSize());
}

TEST(DiskTest, BlockCount)
{
    MockDisk disk;

    disk.SetBlockCount(0x1234567887654321);
    EXPECT_EQ(0x1234567887654321U, disk.GetBlockCount());
}

TEST(DiskTest, SetSectorSizeInBytes)
{
    MockDisk disk;

    EXPECT_TRUE(disk.SetSectorSizeInBytes(512));
    EXPECT_FALSE(disk.SetSectorSizeInBytes(520));
}

TEST(DiskTest, ChangeSectorSize)
{
    MockDisk disk;

    disk.SetSectorSizeInBytes(1024);
    disk.ChangeSectorSize(1024);
    EXPECT_EQ(1024U, disk.GetSectorSizeInBytes());

    EXPECT_THROW(disk.ChangeSectorSize(513), scsi_exception);
    EXPECT_EQ(1024U, disk.GetSectorSizeInBytes());

    disk.SetBlockCount(10);
    EXPECT_CALL(disk, FlushCache());
    disk.ChangeSectorSize(512);
    EXPECT_EQ(512U, disk.GetSectorSizeInBytes());
}

TEST(DiskTest, GetStatistics)
{
    MockDisk disk;

    EXPECT_EQ(2U, disk.GetStatistics().size());
}
