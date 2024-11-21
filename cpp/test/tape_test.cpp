//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

pair<shared_ptr<MockAbstractController>, shared_ptr<Tape>> CreateTape()
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(0);
    auto tape = make_shared<Tape>(0);
    EXPECT_TRUE(tape->Init( { }));
    EXPECT_TRUE(controller->AddDevice(tape));

    return {controller, tape};
}

static void ValidateModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(6U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(12U, pages[1].size());
    EXPECT_EQ(16U, pages[2].size());
    EXPECT_EQ(8U, pages[10].size());
    EXPECT_EQ(16U, pages[15].size());
    EXPECT_EQ(16U, pages[16].size());
    EXPECT_EQ(8U, pages[17].size());
}

static void CheckPosition(AbstractController &controller, PrimaryDevice &tape, uint32_t position)
{
    fill_n(controller.GetBuffer().begin(), 20, 0xff);
    tape.Dispatch(scsi_command::read_position);
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 4)) << "Wrong first block location";
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 8)) << "Wrong last block location";
}

static void CreateTapeFile(Tape &tape, size_t size = 4096)
{
    const auto &filename = CreateTempFile(size);
    tape.SetFilename(filename.string());
    tape.Open();
    tape.Dispatch(scsi_command::rewind);
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
    MockTape tape;

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
}

TEST(TapeTest, ReadData)
{
    vector<uint8_t> buf(4);
    MockTape tape;

    tape.SetReady(true);
    tape.SetBlockCount(1);
    const auto &filename = CreateTempFile(4, "tar");
    tape.SetFilename(filename.string());
    tape.ValidateFile();
    EXPECT_NO_THROW(tape.ReadData(buf));
}

TEST(TapeTest, Unload)
{
    MockTape tape;

    tape.SetReady(true);
    EXPECT_TRUE(tape.Eject(false));
    EXPECT_FALSE(tape.IsReady());
}

TEST(TapeTest, Read6)
{
    auto [controller, tape] = CreateTape();

    // Non-fixed, 0 bytes
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read6));

    // Fixed, 0 bytes
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read6));

    // Fixed and SILI
    controller->SetCdbByte(1, 0x03);
    TestShared::Dispatch(*tape, scsi_command::read6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Non-fixed, 1 byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::read6, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, Write6)
{
    auto [controller, tape] = CreateTape();

    // Non-fixed
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write6));

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write6));

    // Non-fixed, 1 byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::write6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    CreateTapeFile(*tape);
    TestShared::Dispatch(*tape, scsi_command::write6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Non-fixed, 4 bytes
    controller->SetCdbByte(4, 4);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write6));
}

TEST(TapeTest, Erase6)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape, 4567);

    tape->SetProtected(true);
    TestShared::Dispatch(*tape, scsi_command::erase6, sense_key::data_protect, asc::write_protected);

    tape->SetProtected(false);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::erase6));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";

    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::erase6));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 8);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_block_limits));
    EXPECT_EQ(4096U, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(4U, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Rewind)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape, 600);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::erase6));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 1);
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    controller->SetCdbByte(1, 0x00);
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Space6)
{
    auto [controller, tape] = CreateTape();

    // BLOCK, count = 0
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space6));

    // BLOCK, count < 0
    controller->SetCdbByte(2, 0xff);
    TestShared::Dispatch(*tape, scsi_command::space6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BLOCK, count > 0
    controller->SetCdbByte(2, 0x01);
    TestShared::Dispatch(*tape, scsi_command::space6, sense_key::medium_error, asc::read_error);

    // End-of-data
    controller->SetCdbByte(1, 0b011);
    TestShared::Dispatch(*tape, scsi_command::space6, sense_key::medium_error, asc::read_error);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*tape, scsi_command::space6, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, ReadPosition)
{
    auto [controller, tape] = CreateTape();

    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b11000000, controller->GetBuffer()[0]) << "BOP and EOP must be set";
}

TEST(TapeTest, GetBlockSizes)
{
    Tape tape(0);

    const auto &sizes = tape.GetSupportedBlockSizes();
    EXPECT_EQ(5U, sizes.size());

    EXPECT_TRUE(sizes.contains(256));
    EXPECT_TRUE(sizes.contains(512));
    EXPECT_TRUE(sizes.contains(1024));
    EXPECT_TRUE(sizes.contains(2048));
    EXPECT_TRUE(sizes.contains(4096));
}

TEST(TapeTest, SetUpModePages)
{
    map<int, vector<byte>> pages;
    MockTape tape;

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
