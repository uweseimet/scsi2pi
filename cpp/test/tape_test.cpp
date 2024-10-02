//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

static void SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(6U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[0].size());
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(16U, pages[16].size());
    EXPECT_EQ(8U, pages[17].size());
}

TEST(TapeTest, Device_Defaults)
{
    Tape tape(0, false);

    EXPECT_EQ(SCTP, tape.GetType());
    EXPECT_TRUE(tape.SupportsFile());
    EXPECT_FALSE(tape.SupportsParams());
    EXPECT_TRUE(tape.IsProtectable());
    EXPECT_FALSE(tape.IsProtected());
    EXPECT_FALSE(tape.IsReadOnly());
    EXPECT_TRUE(tape.IsRemovable());
    EXPECT_FALSE(tape.IsRemoved());
    EXPECT_TRUE(tape.IsLockable());
    EXPECT_FALSE(tape.IsLocked());
    EXPECT_FALSE(tape.IsStoppable());
    EXPECT_FALSE(tape.IsStopped());

    EXPECT_EQ("SCSI2Pi", tape.GetVendor());
    EXPECT_EQ("SCSI TAPE", tape.GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), tape.GetRevision());
}

TEST(TapeTest, InitDevice)
{
    MockTape tape(0, false);

    EXPECT_TRUE(tape.InitDevice());
}

TEST(TapeTest, Inquiry)
{
    TestShared::Inquiry(SCTP, device_type::sequential_access, scsi_level::scsi_2, "SCSI2Pi SCSI TAPE       ", 0x1f,
        true);
}

TEST(TapeTest, ValidateFile)
{
    MockTape tape(0, false);

    EXPECT_THROW(tape.ValidateFile(), io_exception)<< "Invalid block count";

    tape.SetBlockCount(1);
    EXPECT_THROW(tape.ValidateFile(), io_exception)<< "Missing filename";

    const auto &filename = CreateTempFile(1);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.ValidateFile());
}

TEST(TapeTest, Open)
{
    MockTape tape(0, false);

    EXPECT_THROW(tape.Open(), io_exception);

    const auto &filename = CreateTempFile(4096);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.Open());
}

TEST(TapeTest, Read)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    TestShared::Dispatch(*tape, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Fixed und SILI
    controller->SetCdbByte(1, 0x03);
    TestShared::Dispatch(*tape, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read6));
}

TEST(TapeTest, Write)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    TestShared::Dispatch(*tape, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Fixed und SILI
    controller->SetCdbByte(1, 0x03);
    TestShared::Dispatch(*tape, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write6));
}

TEST(TapeTest, ReadData)
{
    vector<uint8_t> buf(1);

    MockTape tape_tar(0, true);
    tape_tar.SetReady(true);
    EXPECT_THROW(tape_tar.ReadData(buf), scsi_exception);

    MockTape tape_tap(0, false);
    tape_tap.SetReady(true);
    EXPECT_THROW(tape_tap.ReadData(buf), scsi_exception);
}

TEST(TapeTest, WriteData)
{
    vector<uint8_t> buf(1);

    MockTape tape_tar(0, true);
    tape_tar.SetReady(true);
    EXPECT_THROW(tape_tar.WriteData(buf, scsi_command::cmd_write6), scsi_exception);

    MockTape tape_tap(0, false);
    tape_tap.SetReady(true);
    EXPECT_THROW(tape_tap.WriteData(buf, scsi_command::cmd_write6), scsi_exception);
}

TEST(TapeTest, Erase)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    tape->SetProtected(true);
    TestShared::Dispatch(*tape, scsi_command::cmd_erase, sense_key::data_protect, asc::write_protected);

    tape->SetProtected(false);
    TestShared::Dispatch(*tape, scsi_command::cmd_erase, sense_key::medium_error, asc::write_fault);

    const auto &filename = CreateTempFile(4567);
    tape->SetFilename(filename.string());
    EXPECT_NO_THROW(tape->Open());

    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_erase));
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read_position));
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";
    EXPECT_EQ(0, GetInt32(controller->GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(0, GetInt32(controller->GetBuffer(), 8)) << "Wrong last block location";

    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_rewind));
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_erase));
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read_position));
    EXPECT_EQ(0b01000000, controller->GetBuffer()[0]) << "EOP must be set";
    EXPECT_EQ(8, GetInt32(controller->GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(8, GetInt32(controller->GetBuffer(), 8)) << "Wrong last block location";
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, tape] = CreateDevice(SCTP);

    memset(controller->GetBuffer().data(), 0xff, 6);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read_block_limits));
    EXPECT_EQ(0, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(0, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Space)
{
    auto [_, tape_tar] = CreateDevice(SCTP, 0, "test.tar");

    TestShared::Dispatch(*tape_tar, scsi_command::cmd_space, sense_key::illegal_request,
        asc::invalid_command_operation_code);

    auto [controller, tape_tap] = CreateDevice(SCTP, 0, "test.tap");

    // BLOCK, count = 0
    EXPECT_NO_THROW(tape_tap->Dispatch(scsi_command::cmd_space));

    // BLOCK, count < 0
    controller->SetCdbByte(2, 0xff);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_space, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BLOCK, count > 0
    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_space, sense_key::medium_error, asc::read_fault);

    // End-of-data
    controller->SetCdbByte(1, 0b011);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_space, sense_key::medium_error, asc::read_fault);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_space, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, WriteFileMarks)
{
    auto [controller, tape] = CreateDevice(SCTP);

    // Setmarks are not supported
    controller->SetCdbByte(1, 0b010);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks, sense_key::illegal_request,
        asc::invalid_field_in_cdb);

    // Count = 0
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write_filemarks));

    // Count > 0
    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks, sense_key::medium_error, asc::write_fault);

    tape->SetProtected(true);
    controller->SetCdbByte(1, 0b001);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, Locate)
{
    auto [controller, tape_tap] = CreateDevice(SCTP);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_locate, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(1, 0);
    TestShared::Dispatch(*tape_tap, scsi_command::cmd_locate, sense_key::medium_error, asc::read_fault);

    auto [_, tape_tar] = CreateDevice(SCTP, 0, "test.tar");

    EXPECT_NO_THROW(tape_tar->Dispatch(scsi_command::cmd_locate));
}

TEST(TapeTest, ReadPosition)
{
    auto [controller_tap, tape_tap] = CreateDevice(SCTP);

    memset(controller_tap->GetBuffer().data() + 2 * sizeof(uint32_t), 0xff, 8);
    EXPECT_NO_THROW(tape_tap->Dispatch(scsi_command::cmd_read_position));
    EXPECT_EQ(0b11000000, controller_tap->GetBuffer()[0]) << "BOP and EOP must be set";
    EXPECT_EQ(0, GetInt32(controller_tap->GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(0, GetInt32(controller_tap->GetBuffer(), 8)) << "Wrong last block location";

    auto [controller_tar, tape_tar] = CreateDevice(SCTP, 0, "test.tar");

    controller_tar->SetCdbByte(6, 123);
    EXPECT_NO_THROW(tape_tar->Dispatch(scsi_command::cmd_locate));
    EXPECT_NO_THROW(tape_tar->Dispatch(scsi_command::cmd_read_position));
    EXPECT_EQ(0b11000000, controller_tap->GetBuffer()[0]) << "BOP and EOP must be set";
    EXPECT_EQ(123, GetInt32(controller_tar->GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(123, GetInt32(controller_tar->GetBuffer(), 8)) << "Wrong last block location";
}

TEST(TapeTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockTape tape(0, false);

    // Non changeable
    tape.SetUpModePages(pages, 0x3f, false);
    SetUpModePages(pages);

    // Changeable
    pages.clear();
    tape.SetUpModePages(pages, 0x3f, true);
    SetUpModePages(pages);
}

TEST(TapeTest, GetStatistics)
{
    Tape tape(0, false);

    const auto &statistics = tape.GetStatistics();
    EXPECT_EQ(4U, statistics.size());
    EXPECT_EQ("block_read_count", statistics[0].key());
    EXPECT_EQ(0, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("block_write_count", statistics[1].key());
    EXPECT_EQ(0, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
    EXPECT_EQ("read_error_count", statistics[2].key());
    EXPECT_EQ(0, statistics[2].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[2].category());
    EXPECT_EQ("write_error_count", statistics[3].key());
    EXPECT_EQ(0, statistics[3].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[3].category());
}
