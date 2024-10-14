//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

static void ValidateModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(5U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(16U, pages[16].size());
    EXPECT_EQ(8U, pages[17].size());
}

static void CheckPosition(AbstractController &controller, PrimaryDevice &tape, uint32_t position)
{
    memset(controller.GetBuffer().data() + 4 + 2 * sizeof(uint32_t), 0xff, 8);
    tape.Dispatch(scsi_command::cmd_read_position);
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 8)) << "Wrong last block location";
}

static void CreateTapeFile(Tape &tape, size_t size = 4096)
{
    const auto &filename = CreateTempFile(size);
    tape.SetFilename(filename.string());
    tape.Open();
    tape.Dispatch(scsi_command::cmd_format_medium);
}

TEST(TapeTest, Device_Defaults)
{
    Tape tape(0);

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

TEST(TapeTest, SetUp)
{
    Tape tape(0);

    EXPECT_TRUE(tape.SetUp());
}

TEST(TapeTest, Inquiry)
{
    TestShared::Inquiry(SCTP, device_type::sequential_access, scsi_level::scsi_2, "SCSI2Pi SCSI TAPE       ", 0x1f,
        true);
}

TEST(TapeTest, ValidateFile)
{
    MockTape tape(0);

    EXPECT_THROW(tape.ValidateFile(), io_exception)<< "Invalid block count";

    tape.SetBlockCount(1);
    EXPECT_THROW(tape.ValidateFile(), io_exception)<< "Missing filename";

    const auto &filename = CreateTempFile(1);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.ValidateFile());
}

TEST(TapeTest, Open)
{
    Tape tape(0);

    EXPECT_THROW(tape.Open(), io_exception);

    const auto &filename = CreateTempFile(4096);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.Open());
}

TEST(TapeTest, ReadData)
{
    vector<uint8_t> buf(1);
    MockTape tape(0);

    tape.SetReady(true);
    tape.SetBlockCount(1);
    auto filename = CreateTempFile(1, "tap");
    tape.SetFilename(filename.string());
    tape.ValidateFile();
    EXPECT_THROW(tape.ReadData(buf), scsi_exception);

    tape.CleanUp();
    filename = CreateTempFile(1, "tar");
    tape.SetFilename(filename.string());
    tape.ValidateFile();
    EXPECT_NO_THROW(tape.ReadData(buf));
}

TEST(TapeTest, Unload)
{
    MockTape tape(0);

    tape.SetReady(true);
    EXPECT_TRUE(tape.Eject(false));
    EXPECT_FALSE(tape.IsReady());
}

TEST(TapeTest, Read6)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    // Non-fixed
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read6));

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read6));

    // Fixed and SILI
    controller->SetCdbByte(1, 0x03);
    TestShared::Dispatch(*tape, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Non-fixed, one byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    CreateTapeFile(*tape);
    TestShared::Dispatch(*tape, scsi_command::cmd_read6, sense_key::blank_check, asc::no_additional_sense_information);
}

TEST(TapeTest, Write6)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    // Non-fixed
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write6));

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write6));

    // Non-fixed, one byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write6));
}

TEST(TapeTest, Erase6)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    tape->SetProtected(true);
    TestShared::Dispatch(*tape, scsi_command::cmd_erase6, sense_key::data_protect, asc::write_protected);

    tape->SetProtected(false);
    TestShared::Dispatch(*tape, scsi_command::cmd_erase6, sense_key::medium_error, asc::write_error);

    CreateTapeFile(*tape, 4567);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_erase6));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";

    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_rewind));
    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_erase6));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 8);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_read_block_limits));
    EXPECT_EQ(4096U, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(1U, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Rewind)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    CreateTapeFile(*tape, 600);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_rewind));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_erase6));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 1);
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_rewind));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Space6)
{
    auto [controller, tape] = CreateDevice(SCTP);

    // BLOCK, count = 0
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_space6));

    // BLOCK, count < 0
    controller->SetCdbByte(2, 0xff);
    TestShared::Dispatch(*tape, scsi_command::cmd_space6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BLOCK, count > 0
    controller->SetCdbByte(2, 0x01);
    TestShared::Dispatch(*tape, scsi_command::cmd_space6, sense_key::medium_error, asc::read_error);

    // End-of-data
    controller->SetCdbByte(1, 0b011);
    TestShared::Dispatch(*tape, scsi_command::cmd_space6, sense_key::medium_error, asc::read_error);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*tape, scsi_command::cmd_space6, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, WriteFileMarks6)
{
    auto [controller, tape] = CreateDevice(SCTP);

    // Setmarks are not supported
    controller->SetCdbByte(1, 0b010);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks6, sense_key::illegal_request,
        asc::invalid_field_in_cdb);

    // Count = 0
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_write_filemarks6));

    // Count > 0
    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks6, sense_key::medium_error, asc::write_error);

    tape->SetProtected(true);
    controller->SetCdbByte(1, 0b001);
    TestShared::Dispatch(*tape, scsi_command::cmd_write_filemarks6, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, Locate10)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::cmd_locate10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(1, 0);
    TestShared::Dispatch(*tape, scsi_command::cmd_locate10, sense_key::medium_error, asc::read_error);

    CreateTapeFile(*tape);

    TestShared::Dispatch(*tape, scsi_command::cmd_locate10, sense_key::blank_check,
        asc::no_additional_sense_information);

    // BT
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_locate10));
}

TEST(TapeTest, Locate16)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::cmd_locate16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(1, 0);
    TestShared::Dispatch(*tape, scsi_command::cmd_locate16, sense_key::medium_error, asc::read_error);

    CreateTapeFile(*tape);
    TestShared::Dispatch(*tape, scsi_command::cmd_locate16, sense_key::blank_check,
        asc::no_additional_sense_information);

    // BT
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_locate16));
}

TEST(TapeTest, ReadPosition)
{
    auto [controller, tape] = CreateDevice(SCTP);

    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b11000000, controller->GetBuffer()[0]) << "BOP and EOP must be set";
}

TEST(TapeTest, FormatMedium)
{
    auto [controller, device] = CreateDevice(SCTP);
    auto tape = dynamic_pointer_cast<Tape>(device);

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::cmd_format_medium));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockTape tape(0);

    // Non changeable
    tape.SetUpModePages(pages, 0x3f, false);
    ValidateModePages(pages);

    // Changeable
    pages.clear();
    tape.SetUpModePages(pages, 0x3f, true);
    ValidateModePages(pages);

    pages.clear();
    tape.SetUpModePages(pages, 0x00, false);
    EXPECT_EQ(byte { 0x0b }, pages.at(0)[0]);
    EXPECT_EQ(byte { 0x00 }, pages.at(0)[2]);

    pages.clear();
    tape.SetProtected(true);
    tape.SetUpModePages(pages, 0x00, false);
    EXPECT_EQ(byte { 0x80 }, pages.at(0)[2]);
}

TEST(TapeTest, GetStatistics)
{
    Tape tape(0);

    const auto &statistics = tape.GetStatistics();
    EXPECT_EQ(4U, statistics.size());
    EXPECT_EQ("block_read_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("block_write_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
    EXPECT_EQ("read_error_count", statistics[2].key());
    EXPECT_EQ(0U, statistics[2].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[2].category());
    EXPECT_EQ("write_error_count", statistics[3].key());
    EXPECT_EQ(0U, statistics[3].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[3].category());
}
