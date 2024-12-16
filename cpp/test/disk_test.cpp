//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/disk.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

pair<shared_ptr<MockAbstractController>, shared_ptr<NiceMock<MockDisk>>> CreateDisk()
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(0);
    auto disk = make_shared<NiceMock<MockDisk>>();
    EXPECT_TRUE(disk->Init());
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
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::test_unit_ready));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    disk->SetMediumChanged(true);
    Dispatch(disk, scsi_command::test_unit_ready, sense_key::unit_attention, asc::not_ready_to_ready_change);
    EXPECT_FALSE(disk->IsMediumChanged());
}

TEST(DiskTest, ValidateFile)
{
    NiceMock<MockDisk> disk;

    EXPECT_THROW(disk.ValidateFile(), io_exception)<< "Device has 0 blocks";

    disk.SetBlockCount(1);
    disk.SetFilename(CreateImageFile(disk, 512));
    EXPECT_NO_THROW(disk.ValidateFile());
}

TEST(DiskTest, Rezero)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::rezero, sense_key::not_ready, asc::medium_not_present,
        "REZERO must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::rezero));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, FormatUnit)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::format_unit, sense_key::not_ready, asc::medium_not_present,
        "FORMAT UNIT must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::format_unit));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // FMTDATA
    controller->SetCdbByte(1, 0x10);
    Dispatch(disk, scsi_command::format_unit, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(DiskTest, ReassignBlocks)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::reassign_blocks, sense_key::not_ready, asc::medium_not_present,
        "REASSIGN must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::reassign_blocks));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, Seek6)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::seek_6, sense_key::illegal_request, asc::lba_out_of_range,
        "SEEK(6) must fail for a medium with 0 sectors");

    disk->SetBlockCount(1);
    // Sector count
    controller->SetCdbByte(4, 1);
    Dispatch(disk, scsi_command::seek_6, sense_key::not_ready, asc::medium_not_present,
        "SEEK(6) must fail because drive is not ready");

    disk->SetReady(true);

    // Sector count
    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::seek_6));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, Seek10)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::seek_10, sense_key::illegal_request, asc::lba_out_of_range,
        "SEEK(10) must fail for a medium with 0 sectors");

    disk->SetBlockCount(1);
    // Sector count
    controller->SetCdbByte(5, 1);
    Dispatch(disk, scsi_command::seek_10, sense_key::not_ready, asc::medium_not_present,
        "SEEK(10) must fail because drive is not ready");

    disk->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::seek_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, ReadCapacity10)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_capacity_10, sense_key::not_ready, asc::medium_not_present,
        "READ CAPACITY(10) must fail because drive is not ready");

    disk->SetReady(true);

    Dispatch(disk, scsi_command::read_capacity_10, sense_key::illegal_request, asc::medium_not_present,
        "READ CAPACITY(10) must fail because the medium has no capacity");

    disk->SetBlockCount(0x12345678);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_capacity_10));
    auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x1234, GetInt16(buf, 0));
    EXPECT_EQ(0x5677, GetInt16(buf, 2));

    disk->SetBlockCount(0x1234567887654321);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_capacity_10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0xffff, GetInt16(buf, 0));
    EXPECT_EQ(0xffff, GetInt16(buf, 2));
}

TEST(DiskTest, ReadCapacity16)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Neither READ CAPACITY(16) nor READ LONG(16)");

    // Service action: READ CAPACITY(16), not READ LONG(16)
    controller->SetCdbByte(1, 0x10);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::not_ready, asc::medium_not_present,
        "READ CAPACITY(16) must fail because drive is not ready");

    // Service action: READ CAPACITY(16), not READ LONG(16)
    controller->SetCdbByte(1, 0x10);
    disk->SetReady(true);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::illegal_request, asc::medium_not_present,
        "READ CAPACITY(16) must fail because the medium has no capacity");

    // Service action: READ CAPACITY(16), not READ LONG(16)
    controller->SetCdbByte(1, 0x10);
    disk->SetBlockCount(0x1234567887654321);
    disk->SetBlockSize(1024);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_capacity_16_read_long_16));
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x1234, GetInt16(buf, 0));
    EXPECT_EQ(0x5678, GetInt16(buf, 2));
    EXPECT_EQ(0x8765, GetInt16(buf, 4));
    EXPECT_EQ(0x4320, GetInt16(buf, 6));
    EXPECT_EQ(0x0000, GetInt16(buf, 8));
    EXPECT_EQ(0x0400, GetInt16(buf, 10));
}

TEST(DiskTest, ReadFormatCapacities)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_format_capacities, sense_key::not_ready, asc::medium_not_present,
        "READ FORMAT CAPACITIES must fail because drive is not ready");

    disk->SetReady(true);
    disk->SetBlockCount(8192);
    disk->SetBlockSize(512);
    // Allocation length
    controller->SetCdbByte(8, 255);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_format_capacities));
    auto &buf = controller->GetBuffer();
    EXPECT_EQ(40U, GetInt32(buf, 0));
    EXPECT_EQ(disk->GetBlockCount(), GetInt32(buf, 4));
    EXPECT_EQ(disk->GetBlockSize(), GetInt32(buf, 8));
    EXPECT_EQ(8192U, GetInt32(buf, 12));
    EXPECT_EQ(512U, GetInt32(buf, 16));
    EXPECT_EQ(4096U, GetInt32(buf, 20));
    EXPECT_EQ(1024U, GetInt32(buf, 24));
    EXPECT_EQ(2048U, GetInt32(buf, 28));
    EXPECT_EQ(2048U, GetInt32(buf, 32));
    EXPECT_EQ(1024U, GetInt32(buf, 36));
    EXPECT_EQ(4096U, GetInt32(buf, 40));

    disk->SetReadOnly(true);
    // Allocation length
    controller->SetCdbByte(8, 255);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_format_capacities));
    buf = controller->GetBuffer();
    EXPECT_EQ(8U, GetInt32(buf, 0));
    EXPECT_EQ(disk->GetBlockCount(), GetInt32(buf, 4));
    EXPECT_EQ(disk->GetBlockSize(), GetInt32(buf, 8));
}

TEST(DiskTest, Read6)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_6, sense_key::illegal_request, asc::lba_out_of_range,
        "READ(6) must fail for a medium with 0 blocks");

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_6));

    controller->SetCdbByte(4, 2);
    Dispatch(disk, scsi_command::read_6, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Read10)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_10, sense_key::illegal_request, asc::lba_out_of_range,
        "READ(10) must fail for a medium with 0 blocks");

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    controller->SetCdbByte(8, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_10));

    controller->SetCdbByte(8, 2);
    Dispatch(disk, scsi_command::read_10, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Read16)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::read_16, sense_key::illegal_request, asc::lba_out_of_range,
        "READ(16) must fail for a medium with 0 blocks");

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    controller->SetCdbByte(13, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_16));

    controller->SetCdbByte(13, 2);
    Dispatch(disk, scsi_command::read_16, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Write6)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::write_6, sense_key::illegal_request, asc::lba_out_of_range,
        "WRITE(6) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    disk->SetReady(true);
    disk->SetProtectable(true);
    disk->SetProtected(true);
    Dispatch(disk, scsi_command::write_6, sense_key::data_protect, asc::write_protected,
        "WRITE(6) must fail because drive is write-protected");

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    disk->SetProtected(false);
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_6));
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_6));
    EXPECT_EQ(512, controller->GetRemainingLength());
    EXPECT_NO_THROW(disk->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));

    controller->SetCdbByte(4, 2);
    Dispatch(disk, scsi_command::write_6, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Write10)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::write_10, sense_key::illegal_request, asc::lba_out_of_range,
        "WRITE(10) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    disk->SetProtected(false);
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_10));
    controller->SetCdbByte(8, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_10));
    EXPECT_EQ(512, controller->GetRemainingLength());
    EXPECT_NO_THROW(disk->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));

    controller->SetCdbByte(8, 2);
    Dispatch(disk, scsi_command::write_10, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Write16)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::write_16, sense_key::illegal_request, asc::lba_out_of_range,
        "WRITE(16) must fail for a medium with 0 blocks");

    disk->SetBlockCount(1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_16));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    EXPECT_EQ(0U, disk->GetNextSector());

    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    disk->SetProtected(false);
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_16));
    controller->SetCdbByte(13, 1);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_16));
    EXPECT_EQ(512, controller->GetRemainingLength());
    EXPECT_NO_THROW(disk->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));

    controller->SetCdbByte(13, 2);
    Dispatch(disk, scsi_command::write_16, sense_key::illegal_request, asc::lba_out_of_range);
}

TEST(DiskTest, Verify10)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::verify_10, sense_key::illegal_request, asc::lba_out_of_range,
        "VERIFY(10) must fail for a medium with 0 blocks");

    disk->SetReady(true);
    // Verify 0 sectors
    disk->SetBlockCount(1);
    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::verify_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, Verify16)
{
    auto [controller, disk] = CreateDisk();

    Dispatch(disk, scsi_command::verify_16, sense_key::illegal_request, asc::lba_out_of_range,
        "VERIFY(16) must fail for a medium with 0 blocks");

    disk->SetReady(true);
    // Verify 0 sectors
    disk->SetBlockCount(1);
    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::verify_16));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, ReadLong10)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_long_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(1, 1);
    Dispatch(disk, scsi_command::read_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "READ LONG(10) must fail because the RelAdr bit is set");

    controller->SetCdbByte(2, 1);
    Dispatch(disk, scsi_command::read_long_10, sense_key::illegal_request, asc::lba_out_of_range,
        "READ LONG(10) must fail because the capacity is exceeded");

    controller->SetCdbByte(7, 0xff);
    Dispatch(disk, scsi_command::read_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "READ LONG(10) must fail because it only supports a limited transfer length");

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    // 4 Bytes
    controller->SetCdbByte(8, 0x04);
    Dispatch(disk, scsi_command::read_long_10);

    // 512 Bytes
    controller->SetCdbByte(7, 0x02);
    Dispatch(disk, scsi_command::read_long_10);

    // 516 Bytes
    controller->SetCdbByte(7, 0x02);
    controller->SetCdbByte(8, 0x04);
    Dispatch(disk, scsi_command::read_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb);
    RequestSense(controller, disk);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0x20, controller->GetBuffer()[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(4U, GetInt32(controller->GetBuffer(), 3));
}

TEST(DiskTest, ReadLong16)
{
    auto [controller, disk] = CreateDisk();

    // Service action: READ LONG(16), not READ CAPACITY(16)
    controller->SetCdbByte(1, 0x11);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_capacity_16_read_long_16));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(1, 0x11);
    controller->SetCdbByte(2, 1);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::illegal_request, asc::lba_out_of_range,
        "READ LONG(16) must fail because the capacity is exceeded");

    controller->SetCdbByte(1, 0x11);
    controller->SetCdbByte(12, 0xff);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "READ LONG(16) must fail because it only supports a limited transfer length");

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    // 4 Bytes
    controller->SetCdbByte(1, 0x11);
    controller->SetCdbByte(13, 0x04);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16);

    // 512 Bytes
    controller->SetCdbByte(1, 0x11);
    controller->SetCdbByte(13, 0x02);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16);

    // 516 Bytes
    controller->SetCdbByte(1, 0x11);
    controller->SetCdbByte(12, 0x02);
    controller->SetCdbByte(13, 0x04);
    Dispatch(disk, scsi_command::read_capacity_16_read_long_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
    RequestSense(controller, disk);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0x20, controller->GetBuffer()[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(4U, GetInt32(controller->GetBuffer(), 3));
}

TEST(DiskTest, WriteLong10)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_long_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(1, 1);
    Dispatch(disk, scsi_command::write_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "WRITE LONG(10) must fail because the RelAdr bit is set");

    controller->SetCdbByte(2, 1);
    Dispatch(disk, scsi_command::write_long_10, sense_key::illegal_request, asc::lba_out_of_range,
        "WRITE LONG(10) must fail because the capacity is exceeded");

    controller->SetCdbByte(7, 0xff);
    Dispatch(disk, scsi_command::write_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "WRITE LONG(10) must fail because it only supports a limited transfer length");

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    // 4 Bytes
    controller->SetCdbByte(8, 0x04);
    Dispatch(disk, scsi_command::write_long_10);

    // 512 Bytes
    controller->SetCdbByte(7, 0x02);
    Dispatch(disk, scsi_command::write_long_10);

    // 516 Bytes
    controller->SetCdbByte(7, 0x02);
    controller->SetCdbByte(8, 0x04);
    Dispatch(disk, scsi_command::write_long_10, sense_key::illegal_request, asc::invalid_field_in_cdb);
    RequestSense(controller, disk);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0x20, controller->GetBuffer()[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(4U, GetInt32(controller->GetBuffer(), 3));
}

TEST(DiskTest, WriteLong16)
{
    auto [controller, disk] = CreateDisk();

    controller->SetCdbByte(2, 1);
    Dispatch(disk, scsi_command::write_long_16, sense_key::illegal_request, asc::lba_out_of_range,
        "WRITE LONG(16) must fail because the capacity is exceeded");

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::write_long_16));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(12, 0xff);
    Dispatch(disk, scsi_command::write_long_16, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "WRITE LONG(16) must fail because it only supports a limited transfer length");

    disk->SetBlockCount(1);
    disk->SetFilename(CreateImageFile(*disk, 512));
    disk->ValidateFile();

    // 4 Bytes
    controller->SetCdbByte(13, 0x04);
    Dispatch(disk, scsi_command::write_long_16);

    // 512 Bytes
    controller->SetCdbByte(13, 0x02);
    Dispatch(disk, scsi_command::write_long_16);

    // 516 Bytes
    controller->SetCdbByte(12, 0x02);
    controller->SetCdbByte(13, 0x04);
    Dispatch(disk, scsi_command::write_long_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
    RequestSense(controller, disk);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0x20, controller->GetBuffer()[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(4U, GetInt32(controller->GetBuffer(), 3));
}

TEST(DiskTest, Eject)
{
    MockDisk disk;

    disk.SetReady(true);
    disk.SetRemovable(true);
    disk.SetLocked(false);
    EXPECT_CALL(disk, FlushCache);
    EXPECT_TRUE(disk.Eject(false));

    disk.SetReady(true);
    EXPECT_CALL(disk, FlushCache);
    EXPECT_TRUE(disk.Eject(true));
}

void ValidateCachingPage(const AbstractController &controller, int offset)
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

    // Caching page
    controller->SetCdbByte(2, 0x08);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::mode_sense_6));
    ValidateCachingPage(*controller, 12);
}

TEST(DiskTest, ModeSense10)
{
    auto [controller, disk] = CreateDisk();

    // Drive must be ready in order to return all data
    disk->SetReady(true);

    controller->SetCdbByte(2, 0x3f);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);

    // Caching page
    controller->SetCdbByte(2, 0x08);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::mode_sense_10));
    ValidateCachingPage(*controller, 16);
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

    EXPECT_THAT([&] {disk.WriteData( vector {static_cast<int>(scsi_command::write_6)}, {}, 0, 0);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::not_ready),
                Property(&scsi_exception::get_asc, asc::medium_not_present)))) << "Disk is not ready";
}

TEST(DiskTest, SynchronizeCache)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::synchronize_cache_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    EXPECT_CALL(*disk, FlushCache);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::synchronize_cache_16));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, ReadDefectData)
{
    auto [controller, disk] = CreateDisk();

    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(disk, scsi_command::read_defect_data_10));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DiskTest, ChangeBlockSize)
{
    MockDisk disk;

    disk.SetBlockSize(1024);
    disk.SetBlockCount(10);
    EXPECT_CALL(disk, FlushCache);
    disk.ChangeBlockSize(512);
    EXPECT_EQ(512U, disk.GetBlockSize());
}

TEST(DiskTest, CachingMode)
{
    MockDisk disk;

    disk.SetCachingMode(PbCachingMode::PISCSI);
    EXPECT_EQ(PbCachingMode::PISCSI, disk.GetCachingMode());

    disk.SetCachingMode(PbCachingMode::LINUX);
    EXPECT_EQ(PbCachingMode::LINUX, disk.GetCachingMode());

    disk.SetCachingMode(PbCachingMode::LINUX_OPTIMIZED);
    EXPECT_EQ(PbCachingMode::LINUX_OPTIMIZED, disk.GetCachingMode());

    disk.SetCachingMode(PbCachingMode::WRITE_THROUGH);
    EXPECT_EQ(PbCachingMode::WRITE_THROUGH, disk.GetCachingMode());
}

TEST(DiskTest, GetStatistics)
{
    MockDisk disk;

    // There is no cache, therefore there are only 2 items
    const auto &statistics = disk.GetStatistics();
    EXPECT_EQ(2U, statistics.size());
    EXPECT_EQ("block_read_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("block_write_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
}
