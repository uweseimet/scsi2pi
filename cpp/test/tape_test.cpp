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
    const bool bt = controller.GetCdb()[1] & 0x01;
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 4)) << "Wrong first " << (bt ? "position" : "block location");
    EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 8)) << "Wrong last " << (bt ? "position" : "block location");
}

static string CreateTapeFile(Tape &tape, size_t size = 4096, const string &extension = "")
{
    const auto &filename = CreateTempFile(size, extension);
    tape.SetFilename(filename.string());
    tape.Open();
    return filename.string();
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

    const auto &filename = CreateTempFile(4096);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.Open());
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
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));

    // Fixed, 0 bytes
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));

    // Fixed and SILI
    controller->SetCdbByte(1, 0x03);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Non-fixed, 1 byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    const string &filename = CreateTapeFile(*tape);
    fstream file(filename);
    const array<uint8_t, META_DATA_SIZE> good_data_non_fixed = { 0x0c, 0x00, 0x00, 0x00 };
    const array<uint8_t, META_DATA_SIZE> good_data_fixed = { 0x00, 0x02, 0x00, 0x00 };
    const array<uint8_t, META_DATA_SIZE> good_data_broken = { 0x00, 0x04, 0x00, 0x00 };
    const array<uint8_t, META_DATA_SIZE> bad_data_recovered = { 0x00, 0x02, 0x00, 0x80 };
    const array<uint8_t, META_DATA_SIZE> bad_data = { 0x00, 0x00, 0x00, 0x80 };
    file.write((const char*)good_data_non_fixed.data(), good_data_non_fixed.size());
    file << "123456789012";
    file.write((const char*)good_data_non_fixed.data(), good_data_non_fixed.size());
    file.write((const char*)good_data_fixed.data(), good_data_fixed.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data_fixed.data(), good_data_fixed.size());
    file.write((const char*)bad_data_recovered.data(), bad_data_recovered.size());
    file.seekp(512, ios::cur);
    file.write((const char*)bad_data_recovered.data(), bad_data_recovered.size());
    file.write((const char*)bad_data.data(), bad_data.size());
    file.write((const char*)good_data_fixed.data(), good_data_fixed.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data_broken.data(), good_data_broken.size());
    file.flush();

    tape->Dispatch(scsi_command::rewind);

    // Non-fixed, 12 bytes
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));
    EXPECT_EQ('1', controller->GetBuffer()[0]);
    EXPECT_EQ('2', controller->GetBuffer()[1]);
    EXPECT_EQ('3', controller->GetBuffer()[2]);
    EXPECT_EQ('4', controller->GetBuffer()[3]);
    EXPECT_EQ('5', controller->GetBuffer()[4]);
    EXPECT_EQ('6', controller->GetBuffer()[5]);
    EXPECT_EQ('7', controller->GetBuffer()[6]);
    EXPECT_EQ('8', controller->GetBuffer()[7]);
    EXPECT_EQ('9', controller->GetBuffer()[8]);
    EXPECT_EQ('0', controller->GetBuffer()[9]);
    EXPECT_EQ('1', controller->GetBuffer()[10]);
    EXPECT_EQ('2', controller->GetBuffer()[11]);

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));

    // Fixed, 1 block, bad data recovered
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));

    // Fixed, 1 block, bad data
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::read_error);

    // Fixed, 1 block, trailing length mismatch
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::read_error);

    const array<uint8_t, META_DATA_SIZE> block_size_mismatch = { 0x00, 0x01, 0x00, 0x00 };
    file.seekp(0, ios::beg);
    file.write((const char*)block_size_mismatch.data(), block_size_mismatch.size());
    file.seekp(256, ios::cur);
    file.write((const char*)block_size_mismatch.data(), block_size_mismatch.size());
    file.flush();

    tape->Dispatch(scsi_command::rewind);

    // Fixed, 1 block, block size mismatch
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::no_additional_sense_information);

    tape->Dispatch(scsi_command::rewind);

    // Non-fixed, 4 bytes (less than block size)
    controller->SetCdbByte(4, 4);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::no_additional_sense_information);

    tape->Dispatch(scsi_command::rewind);

    // Non-fixed, 4 bytes (less than block size)
    controller->SetCdbByte(4, 4);
    // SILI
    controller->SetCdbByte(1, 0x02);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_6));

    // Non-fixed, 1024 bytes (more than block size)
    controller->SetCdbByte(3, 0x04);
    controller->SetCdbByte(4, 0x00);
    controller->SetCdbByte(1, 0x00);
    tape->Dispatch(scsi_command::rewind);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::no_additional_sense_information);

    tape->Dispatch(scsi_command::rewind);

    // Non-fixed, 1024 bytes (more than block size)
    controller->SetCdbByte(3, 0x04);
    // SILI
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::read_6, sense_key::medium_error, asc::no_additional_sense_information);

    // TODO Check ILI and information field (REQUEST SENSE)
    // TODO Checking spacing over blocks and finding filemark
}

TEST(TapeTest, Write6)
{
    auto [controller, tape] = CreateTape();

    // Non-fixed
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_6));

    // Fixed
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_6));

    // Non-fixed, 1 byte
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*tape, scsi_command::write_6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Non-fixed, 4 bytes
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 4);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_6));

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_6));
}

TEST(TapeTest, Erase6_simh)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape, 4567);

    tape->SetProtected(true);
    TestShared::Dispatch(*tape, scsi_command::erase_6, sense_key::data_protect, asc::write_protected);

    tape->SetProtected(false);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::erase_6));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";

    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    tape->Dispatch(scsi_command::write_filemarks_6);
    controller->SetCdbByte(4, 0x00);
    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::erase_6));
    controller->SetCdbByte(1, 0x00);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Erase6_tar)
{
    auto [_, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    TestShared::Dispatch(*tape, scsi_command::erase_6, sense_key::illegal_request,
        asc::invalid_command_operation_code);
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::read_block_limits));
    EXPECT_EQ(8192U, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(4U, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Rewind)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape, 600);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    tape->Dispatch(scsi_command::write_filemarks_6);
    controller->SetCdbByte(4, 0x00);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 4);
    controller->SetCdbByte(1, 0x00);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::rewind));
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Space6_simh)
{
    auto [controller, tape] = CreateTape();

    const string &filename = CreateTapeFile(*tape);

    // BLOCK, count = 0
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));

    // BLOCK, count < 0
    controller->SetCdbByte(2, 0xff);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::no_sense, asc::no_additional_sense_information);

    // BLOCK, count > 0
    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::no_sense, asc::no_additional_sense_information);

    // End-of-data, count > 0
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(2, 1);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::medium_error, asc::no_additional_sense_information);

    // End-of-data, count < 0
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(2, 0xff);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::medium_error, asc::no_additional_sense_information);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // Write 5 filemarks and 1 end-of-data
    ofstream file(filename);
    const array<uint8_t, META_DATA_SIZE> filemark = { 0, 0, 0, 0 };
    const array<uint8_t, META_DATA_SIZE> end_of_data = { 'P', '2', 'S', 0x73 };
    file.write((const char*)filemark.data(), filemark.size());
    file.write((const char*)filemark.data(), filemark.size());
    file.write((const char*)filemark.data(), filemark.size());
    file.write((const char*)filemark.data(), filemark.size());
    file.write((const char*)filemark.data(), filemark.size());
    file.write((const char*)end_of_data.data(), end_of_data.size());
    file.flush();

    tape->Dispatch(scsi_command::rewind);

    // Space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 0);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 4);

    // Space over 2 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 2);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 0);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 12);

    // Reverse-space over 2 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(2, 0);
    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 0);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 4);

    // Try to space over 5 filemarks (only 4 are left)
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 5);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::blank_check, asc::no_additional_sense_information);

    // Write 6 data records (bad and good) and different markers, terminated by a filemark
    file.seekp(0, ios::beg);
    const array<uint8_t, META_DATA_SIZE> good_data = { 0x00, 0x02, 0x00, 0x00 };
    const array<uint8_t, META_DATA_SIZE> bad_data = { 0x00, 0x02, 0x00, 0x80 };
    const array<uint8_t, META_DATA_SIZE> bad_data_not_recovered = { 0x00, 0x00, 0x00, 0x80 };
    const array<uint8_t, META_DATA_SIZE> private_marker = { 0x00, 0x00, 0x00, 0x70 };
    const array<uint8_t, META_DATA_SIZE> reserved_marker = { 0x00, 0x00, 0x00, 0xf0 };
    const array<uint8_t, META_DATA_SIZE> erase_gap = { 0xff, 0xff, 0xff, 0x7f };
    const array<uint8_t, META_DATA_SIZE> tape_description_data_record = { 0x01, 0x00, 0x00, 0xe0 };
    file.write((const char*)good_data.data(), good_data.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data.data(), good_data.size());
    file.write((const char*)bad_data_not_recovered.data(), bad_data_not_recovered.size());
    file.write((const char*)good_data.data(), good_data.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data.data(), good_data.size());
    file.write((const char*)bad_data.data(), bad_data.size());
    file.seekp(512, ios::cur);
    file.write((const char*)bad_data.data(), bad_data.size());
    file.write((const char*)good_data.data(), good_data.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data.data(), good_data.size());
    file.write((const char*)erase_gap.data(), erase_gap.size());
    file.write((const char*)private_marker.data(), private_marker.size());
    file.write((const char*)reserved_marker.data(), reserved_marker.size());
    file.write((const char*)tape_description_data_record.data(), tape_description_data_record.size());
    file.seekp(2, ios::cur);
    file.write((const char*)tape_description_data_record.data(), tape_description_data_record.size());
    file.write((const char*)good_data.data(), good_data.size());
    file.seekp(512, ios::cur);
    file.write((const char*)good_data.data(), good_data.size());
    file.write((const char*)filemark.data(), filemark.size());
    file.flush();

    tape->Dispatch(scsi_command::rewind);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 1);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 520);
    controller->SetCdbByte(1, 0);

    // Space over 3 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 3);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 4);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 1564);
    controller->SetCdbByte(1, 0);

    // Reverse-space over 2 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::space_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(2, 0);
    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 2);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 520);
    controller->SetCdbByte(1, 0);

    // Try to space over 6 blocks, in order to hit the filemark
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 6);
    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::no_sense, asc::no_additional_sense_information);
}

TEST(TapeTest, Space6_tar)
{
    auto [_, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    TestShared::Dispatch(*tape, scsi_command::space_6, sense_key::illegal_request,
        asc::invalid_command_operation_code);
}

TEST(TapeTest, WriteFileMarks6_simh)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape, 512);

    // Setmarks are not supported
    controller->SetCdbByte(1, 0b010);
    TestShared::Dispatch(*tape, scsi_command::write_filemarks_6, sense_key::illegal_request,
        asc::invalid_field_in_cdb);

    // Count = 0
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_filemarks_6));

    // Count = 100
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_filemarks_6));
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    CheckPosition(*controller, *tape, 0);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 400);
    controller->SetCdbByte(1, 0);

    // Count = 100
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    TestShared::Dispatch(*tape, scsi_command::write_filemarks_6, sense_key::volume_overflow,
        asc::no_additional_sense_information);
    CheckPosition(*controller, *tape, 0);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 512);

    tape->SetProtected(true);
    controller->SetCdbByte(1, 0b001);
    TestShared::Dispatch(*tape, scsi_command::write_filemarks_6, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, WriteFileMarks6_tar)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::write_filemarks_6));
}

TEST(TapeTest, Locate10_simh)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::no_sense, asc::no_additional_sense_information);

    // BT
    controller->SetCdbByte(1, 0x04);
    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::no_sense, asc::no_additional_sense_information);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 1);
    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, Locate10_tar)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(6, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::locate_10));
    controller->SetCdbByte(6, 0);
    CheckPosition(*controller, *tape, 1);

    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 512);
    controller->SetCdbByte(1, 0);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 123);
    TestShared::Dispatch(*tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(5, 0x02);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::locate_10));
    controller->SetCdbByte(5, 0);
    CheckPosition(*controller, *tape, 1);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 512);
    controller->SetCdbByte(1, 0);
}

TEST(TapeTest, Locate16_simh)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::no_sense, asc::no_additional_sense_information);

    // BT
    controller->SetCdbByte(1, 0x04);
    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::no_sense, asc::no_additional_sense_information);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 1);
    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, Locate16_tar)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(11, 1);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::locate_16));
    controller->SetCdbByte(11, 0);
    CheckPosition(*controller, *tape, 1);

    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 512);
    controller->SetCdbByte(1, 0);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 123);
    TestShared::Dispatch(*tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(10, 0x02);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::locate_16));
    controller->SetCdbByte(10, 0);
    CheckPosition(*controller, *tape, 1);
    // BT
    controller->SetCdbByte(1, 0x01);
    CheckPosition(*controller, *tape, 512);
    controller->SetCdbByte(1, 0);
}

TEST(TapeTest, ReadPosition)
{
    auto [controller, tape] = CreateTape();

    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b11000000, controller->GetBuffer()[0]) << "BOP and EOP must be set";
}

TEST(TapeTest, FormatMedium_simh)
{
    auto [controller, tape] = CreateTape();

    CreateTapeFile(*tape);
    EXPECT_NO_THROW(tape->Dispatch(scsi_command::format_medium));
    CheckPosition(*controller, *tape, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Write a filemark in order to advance the position
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    tape->Dispatch(scsi_command::write_filemarks_6);
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    TestShared::Dispatch(*tape, scsi_command::format_medium, sense_key::illegal_request,
        asc::sequential_positioning_error);

    tape->SetProtected(true);
    TestShared::Dispatch(*tape, scsi_command::format_medium, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, FormatMedium_tar)
{
    auto [controller, tape] = CreateTape();
    CreateTapeFile(*tape, 512, "tar");

    TestShared::Dispatch(*tape, scsi_command::format_medium, sense_key::illegal_request,
        asc::invalid_command_operation_code);
}

TEST(TapeTest, GetBlockSizes)
{
    Tape tape(0);

    const auto &sizes = tape.GetSupportedBlockSizes();
    EXPECT_EQ(5U, sizes.size());

    EXPECT_TRUE(sizes.contains(512));
    EXPECT_TRUE(sizes.contains(1024));
    EXPECT_TRUE(sizes.contains(2048));
    EXPECT_TRUE(sizes.contains(4096));
    EXPECT_TRUE(sizes.contains(8192));
}

TEST(TapeTest, ValidateBlockSize)
{
    MockTape tape;

    EXPECT_FALSE(tape.ValidateBlockSize(0));
    EXPECT_TRUE(tape.ValidateBlockSize(4));
    EXPECT_FALSE(tape.ValidateBlockSize(7));
    EXPECT_TRUE(tape.ValidateBlockSize(512));
    EXPECT_TRUE(tape.ValidateBlockSize(131072));
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
