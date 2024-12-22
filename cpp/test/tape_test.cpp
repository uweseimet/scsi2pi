//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

using namespace memory_util;

#define CheckPositions(device, position, object_location) ({\
    auto c = static_cast<MockAbstractController*>(device->GetController());\
    c->ResetCdb();\
    c->SetCdbByte(1, 0x01);\
    CheckPosition(*c, device, position);\
    c->SetCdbByte(1, 0);\
    CheckPosition(*c, device, object_location);\
})

static void CheckPosition(const AbstractController &controller, shared_ptr<PrimaryDevice> tape, uint32_t position)
{
    fill_n(controller.GetBuffer().begin(), 12, 0xff);
    Dispatch(tape, scsi_command::read_position);

    if (position != GetInt32(controller.GetBuffer(), 4) || position != GetInt32(controller.GetBuffer(), 8)) {
        EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 4));
    }
}

static void CheckMetaData(istream &file, const SimhMetaData &expected)
{
    array<uint8_t, META_DATA_SIZE> data;
    file.read((char*)data.data(), data.size());
    SimhMetaData meta_data = FromLittleEndian(data);
    EXPECT_EQ(expected.cls, meta_data.cls);
    EXPECT_EQ(expected.value, meta_data.value);
    file.seekg(Pad(expected.value), ios::cur);
    file.read((char*)data.data(), data.size());
    meta_data = FromLittleEndian(data);
    EXPECT_EQ(expected.cls, meta_data.cls);
    EXPECT_EQ(expected.value, meta_data.value);
}

pair<shared_ptr<MockAbstractController>, shared_ptr<MockTape>> CreateTape()
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(0);
    auto tape = make_shared<MockTape>();
    tape->SetParams( { });
    EXPECT_TRUE(tape->Init());
    EXPECT_TRUE(controller->AddDevice(tape));

    return {controller, tape};
}

void WriteSimhObject(ostream &file, span<const uint8_t> leading, int length = 0, span<const uint8_t> trailing = { })
{
    assert(!(leading.size() % 4) && !(trailing.size() % 4) && "SIMH meta data length must be a multiple of 4");

    file.write((const char*)leading.data(), leading.size());
    file.seekp(length, ios::cur);
    if (!trailing.empty()) {
        file.write((const char*)trailing.data(), trailing.size());
    }
}

static void WriteGoodData(ostream &file, int length = 512)
{
    const vector<uint8_t> data(length);
    WriteGoodData(file, data, length);
}

static void WriteEndOfData(ostream &file)
{
    const vector<uint8_t> &end_of_data = { 'P', '2', 'S', 0x73 };
    WriteSimhObject(file, end_of_data);
}

static void Rewind(shared_ptr<Tape> tape)
{
    Dispatch(tape, scsi_command::rewind);
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

TEST(TapeTest, Device_Defaults)
{
    Tape tape(0);

    EXPECT_EQ(SCTP, tape.GetType());
    EXPECT_TRUE(tape.SupportsImageFile());
    EXPECT_TRUE(tape.SupportsParams());
    EXPECT_TRUE(tape.IsProtectable());
    EXPECT_FALSE(tape.IsProtected());
    EXPECT_FALSE(tape.IsReadOnly());
    EXPECT_TRUE(tape.IsRemovable());
    EXPECT_FALSE(tape.IsRemoved());
    EXPECT_FALSE(tape.IsLocked());
    EXPECT_FALSE(tape.IsStoppable());
    EXPECT_FALSE(tape.IsStopped());

    const auto& [vendor, product, revision] = tape.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("SCSI TAPE", product);
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST(TapeTest, GetDefaultParams)
{
    Tape tape(0);

    auto params = tape.GetDefaultParams();
    EXPECT_EQ(1U, params.size());
    EXPECT_EQ("0", params["append"]);
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
    tape.SetParams( { });

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
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    Dispatch(tape, scsi_command::read_6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Drive is not in fixed mode, block size is 0");

    const string &filename = CreateImageFile(*tape);

    // Fixed, 0 blocks
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    CheckPositions(tape, 0, 0);

    // Fixed and SILI, 0 blocks
    controller->SetCdbByte(1, 0x03);
    Dispatch(tape, scsi_command::read_6, sense_key::illegal_request, asc::invalid_field_in_cdb);
    CheckPositions(tape, 0, 0);

    fstream file(filename);
    const vector<uint8_t> &good_data_non_fixed = { 0x0c, 0x00, 0x00, 0x00 };
    const vector<uint8_t> &bad_data = { 0x00, 0x02, 0x00, 0x80 };
    const vector<uint8_t> &bad_data_not_recovered = { 0x00, 0x00, 0x00, 0x80 };
    WriteSimhObject(file, good_data_non_fixed);
    file << "123456789012";
    WriteSimhObject(file, good_data_non_fixed);
    WriteGoodData(file);
    WriteSimhObject(file, bad_data, 512, bad_data);
    WriteSimhObject(file, bad_data_not_recovered);
    WriteEndOfData(file);
    file.flush();

    const auto &buf = controller->GetBuffer();

    Rewind(tape);

    // Non-fixed, 12 bytes
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ('1', buf[0]);
    EXPECT_EQ('2', buf[1]);
    EXPECT_EQ('3', buf[2]);
    EXPECT_EQ('4', buf[3]);
    EXPECT_EQ('5', buf[4]);
    EXPECT_EQ('6', buf[5]);
    EXPECT_EQ('7', buf[6]);
    EXPECT_EQ('8', buf[7]);
    EXPECT_EQ('9', buf[8]);
    EXPECT_EQ('0', buf[9]);
    EXPECT_EQ('1', buf[10]);
    EXPECT_EQ('2', buf[11]);
    CheckPositions(tape, 20, 1);

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    CheckPositions(tape, 540, 2);

    // Fixed, 1 block, bad data recovered
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    CheckPositions(tape, 1060, 3);

    // Fixed, 1 block, bad data
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::read_6, sense_key::medium_error, asc::read_error);
    CheckPositions(tape, 1064, 4);

    file.seekp(0);
    WriteGoodData(file, 256);
    file.flush();

    Rewind(tape);

    // Leading length != trailing length
    const vector<uint8_t> &bad_trailing = { 0x01, 0x00, 0x00, 0x00 };
    file.seekp(0);
    WriteSimhObject(file, good_data_non_fixed, good_data_non_fixed[0], bad_trailing);
    file.flush();

    Rewind(tape);

    // Non-fixed, 12 bytes
    controller->SetCdbByte(4, 12);
    Dispatch(tape, scsi_command::read_6, sense_key::medium_error, asc::read_error);

    file.seekp(0);
    WriteFilemark(file);
    file.flush();

    Rewind(tape);

    // Non-fixed, 90 byte
    controller->SetCdbByte(4, 90);
    Dispatch(tape, scsi_command::read_6, sense_key::no_sense, asc::no_additional_sense_information);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(90U, GetInt32(buf, 3));

    Rewind(tape);

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::read_6, sense_key::no_sense, asc::no_additional_sense_information);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0U, GetInt32(buf, 3));
}

TEST(TapeTest, Read6_BlockSizeMismatch)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    fstream file(filename);
    WriteGoodData(file, 256);
    file.flush();

    const auto &buf = controller->GetBuffer();

    // Non-fixed, 1 byte (less than block size)
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ(1, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_TRUE(buf[0] & 0x80) << "VALID must be set";
    EXPECT_TRUE(buf[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(-255, static_cast<int>(GetInt32(buf, 3))) << "Wrong block size mismatch difference";
    CheckPositions(tape, 264, 1);

    Rewind(tape);

    // Non-fixed, 1 byte (less than block size)
    controller->SetCdbByte(4, 1);
    // SILI
    controller->SetCdbByte(1, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ(1, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_FALSE(buf[0] & 0x80) << "VALID must not be set";
    EXPECT_FALSE(buf[2] & 0x20) << "ILI must not be set";
    CheckPositions(tape, 264, 1);

    Rewind(tape);

    // Non-fixed, 1024 bytes (more than block size)
    controller->SetCdbByte(3, 0x04);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ(256, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_TRUE(buf[0] & 0x80) << "VALID must be set";
    EXPECT_TRUE(buf[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(768U, GetInt32(buf, 3)) << "Wrong block size mismatch difference";
    CheckPositions(tape, 264, 1);

    Rewind(tape);

    // Non-fixed, 1024 bytes (more than block size)
    controller->SetCdbByte(3, 0x04);
    // SILI
    controller->SetCdbByte(1, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ(256, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_TRUE(buf[0] & 0x80) << "VALID must be set";
    EXPECT_TRUE(buf[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(768U, GetInt32(buf, 3)) << "Wrong block size mismatch difference";
    CheckPositions(tape, 264, 1);

    file.seekp(0);
    WriteGoodData(file, 1024);
    file.flush();

    Rewind(tape);

    // Non-fixed, 90 bytes (less than block size)
    controller->SetCdbByte(4, 90);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_6));
    EXPECT_EQ(90, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_TRUE(buf[0] & 0x80) << "VALID must be set";
    EXPECT_TRUE(buf[2] & 0x20) << "ILI must be set";
    EXPECT_EQ(-934, static_cast<int>(GetInt32(buf, 3))) << "Wrong block size mismatch difference";
    CheckPositions(tape, 1032, 1);
}

TEST(TapeTest, Read16)
{
    auto [controller, tape] = CreateTape();

    // Partition 1
    controller->SetCdbByte(3, 1);
    Dispatch(tape, scsi_command::read_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
    CheckPositions(tape, 0, 0);

    const string &filename = CreateImageFile(*tape);
    fstream file(filename);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    WriteEndOfData(file);
    file.flush();

    // Fixed, block 2
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 2);
    controller->SetCdbByte(14, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_16));
    CheckPositions(tape, 1560, 3);

    // Fixed, non-existing block 10
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 10);
    controller->SetCdbByte(14, 1);
    Dispatch(tape, scsi_command::read_16, sense_key::no_sense, asc::locate_operation_failure);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
}

TEST(TapeTest, Write6)
{
    auto [controller, tape] = CreateTape();

    // Non-fixed, 0 bytes
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_6));
    CheckPositions(tape, 0, 0);

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    Dispatch(tape, scsi_command::write_6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Drive is not in fixed mode, block size is 0");
    CheckPositions(tape, 0, 0);

    const string &filename = CreateImageFile(*tape);
    ifstream file(filename);

    // Non-fixed, 2 bytes
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 2);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 2));
    CheckMetaData(file, { simh_class::tape_mark_good_data_record, 2 });
    CheckPositions(tape, 10, 1);

    Rewind(tape);
    file.seekg(0);

    // Non-fixed, 1 byte
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 1));
    CheckMetaData(file, { simh_class::tape_mark_good_data_record, 1 });
    CheckPositions(tape, 10, 1);

    Rewind(tape);
    file.seekg(0);

    // Non-fixed, 512 bytes
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(3, 2);
    controller->SetCdbByte(4, 0);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    CheckMetaData(file, { simh_class::tape_mark_good_data_record, 512 });
    CheckPositions(tape, 520, 1);

    Rewind(tape);
    file.seekg(0);

    // Fixed, 1 block
    controller->SetCdbByte(0, static_cast<int>(scsi_command::write_6));
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    CheckMetaData(file, { simh_class::tape_mark_good_data_record, 512 });
    CheckPositions(tape, 520, 1);
}

TEST(TapeTest, Write16)
{
    auto [controller, tape] = CreateTape();

    // FCS/LCS
    controller->SetCdbByte(1, 0b1100);
    Dispatch(tape, scsi_command::write_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
    CheckPositions(tape, 0, 0);

    // Partition 1
    controller->SetCdbByte(3, 1);
    Dispatch(tape, scsi_command::write_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
    CheckPositions(tape, 0, 0);

    const string &filename = CreateImageFile(*tape);
    fstream file(filename);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    file.flush();

    // Fixed, block 2
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 2);
    controller->SetCdbByte(14, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_16));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    file.seekg(1040);
    CheckMetaData(file, { simh_class::tape_mark_good_data_record, 512 });
    CheckPositions(tape, 1560, 3);

    // Fixed, non-existing block 10
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 10);
    controller->SetCdbByte(14, 1);
    Dispatch(tape, scsi_command::write_16, sense_key::no_sense, asc::locate_operation_failure);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
}

TEST(TapeTest, Erase6_simh)
{
    auto [controller, tape] = CreateTape();

    CreateImageFile(*tape, 4567);

    tape->SetProtected(true);
    Dispatch(tape, scsi_command::erase_6, sense_key::data_protect, asc::write_protected);

    tape->SetProtected(false);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::erase_6));
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";

    Rewind(tape);

    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    Dispatch(tape, scsi_command::write_filemarks_6);
    controller->SetCdbByte(4, 0x00);
    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::erase_6));
    controller->SetCdbByte(1, 0x00);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Erase6_tar)
{
    auto [__, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, scsi_command::erase_6, sense_key::illegal_request,
        asc::invalid_command_operation_code);
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, tape] = CreateTape();

    CreateImageFile(*tape);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::read_block_limits));
    EXPECT_EQ(0x02fffffcU, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(4, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Rewind)
{
    auto [controller, tape] = CreateTape();

    CreateImageFile(*tape, 600);
    Rewind(tape);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    Dispatch(tape, scsi_command::write_filemarks_6);
    CheckPositions(tape, 4, 0);
    controller->SetCdbByte(1, 0x00);
    Rewind(tape);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Space6_simh)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    controller->SetCdbByte(2, 1);
    Dispatch(tape, scsi_command::space_6, sense_key::illegal_request, asc::invalid_field_in_cdb);


    // Write 6 filemarks and 1 end-of-data
    ofstream file(filename);
    WriteFilemark(file);
    WriteFilemark(file);
    WriteFilemark(file);
    WriteFilemark(file);
    WriteFilemark(file);
    WriteFilemark(file);
    WriteEndOfData(file);
    file.flush();

    Rewind(tape);

    // Space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 4, 1);

    // Space over 3 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 3);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 16, 4);

    // Reverse-space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 12, 3);

    // Reverse-space over 2 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 4, 1);

    // Try to space over 10 filemarks, hitting end-of-data
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 10);
    Dispatch(tape, scsi_command::space_6, sense_key::blank_check);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(5U, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 24, 6);

    Rewind(tape);

    // Search for end-of-data, the count must be ignored
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(4, 255);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 24, 6);

    Rewind(tape);

    // Space over 1 filemark, then reverse-space over more filemarks than available
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 4, 1);
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0x00);
    Dispatch(tape, scsi_command::space_6, sense_key::no_sense, asc::no_additional_sense_information);
    RequestSense(controller, tape);
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0b01000000, buf[2]) << "EOM must be set";
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(ascq::beginning_of_partition_medium_detected, static_cast<ascq>(buf[13]))
        << "ASCQ must be beginning-of-medium";
    EXPECT_EQ(255U, GetInt32(buf, 3));


    // Write 6 data records (bad and good) and different markers, 1 filemark
    file.seekp(0);
    const vector<uint8_t> &bad_data = { 0x00, 0x02, 0x00, 0x80 };
    const vector<uint8_t> &bad_data_not_recovered = { 0x00, 0x00, 0x00, 0x80 };
    const vector<uint8_t> &private_marker = { 0x00, 0x00, 0x00, 0x70 };
    const vector<uint8_t> &reserved_marker = { 0x00, 0x00, 0x00, 0xf0 };
    const vector<uint8_t> &erase_gap = { 0xfe, 0xff, 0xff, 0xff };
    const vector<uint8_t> &tape_description_data_record = { 0x01, 0x00, 0x00, 0xe0 };
    WriteGoodData(file);
    WriteSimhObject(file, bad_data_not_recovered);
    WriteGoodData(file);
    WriteSimhObject(file, bad_data, 512, bad_data);
    WriteGoodData(file);
    WriteSimhObject(file, erase_gap);
    WriteSimhObject(file, private_marker);
    WriteSimhObject(file, reserved_marker);
    WriteSimhObject(file, tape_description_data_record, 2, tape_description_data_record);
    WriteGoodData(file);
    WriteFilemark(file);
    file.flush();

    Rewind(tape);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 520, 1);

    // Space over 3 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 3);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 1564, 4);

    // Reverse-space over 2 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 524, 2);

    // Try to space over 6 blocks, in order to hit the filemark
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 6);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 2630, 8);

    // Reverse-space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 2626, 7);

    // Try to reverse-space over non-existing filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";


    // Write 1 block, 1 filemark, 1 block, 1 end-of-data
    file.seekp(0);
    WriteGoodData(file);
    WriteFilemark(file);
    WriteGoodData(file);
    WriteEndOfData(file);
    file.flush();

    Rewind(tape);

    // Space over 2 blocks, which hits the filemark
    controller->SetCdbByte(4, 2);
    Dispatch(tape, scsi_command::space_6);
    CheckPositions(tape, 524, 2);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 1044, 3);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::space_6, sense_key::blank_check);
    RequestSense(controller, tape);
    EXPECT_NE(ascq::end_of_data_detected, static_cast<ascq>(controller->GetBuffer()[13]));
    EXPECT_TRUE(controller->GetBuffer()[0] & 0x80);
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 1044, 3);

    Rewind(tape);

    // Space for end-of-data
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::space_6));
    CheckPositions(tape, 1044, 3);
}

TEST(TapeTest, Space6_tar)
{
    auto [___, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, scsi_command::space_6, sense_key::illegal_request,
        asc::invalid_command_operation_code);
}

TEST(TapeTest, WriteFileMarks6_simh)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 1024);

    // Setmarks are not supported
    controller->SetCdbByte(1, 0b010);
    Dispatch(tape, scsi_command::write_filemarks_6, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // 0 filemarks
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_6));

    // 100 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_6));
    CheckPositions(tape, 400, 0);

    // 100 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    Dispatch(tape, scsi_command::write_filemarks_6, sense_key::volume_overflow);
    CheckPositions(tape, 512, 0);

    tape->SetProtected(true);
    controller->SetCdbByte(1, 0b001);
    Dispatch(tape, scsi_command::write_filemarks_6, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, WriteFileMarks6_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_6));
}

TEST(TapeTest, WriteFileMarks16_simh)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 1024);

    // 0 filemarks
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_16));

    // 100 filemarks
    controller->SetCdbByte(14, 100);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_16));
    CheckPositions(tape, 400, 0);

    // 100 filemarks
    controller->SetCdbByte(14, 100);
    Dispatch(tape, scsi_command::write_filemarks_16, sense_key::volume_overflow);
    CheckPositions(tape, 512, 0);

    tape->SetProtected(true);
    Dispatch(tape, scsi_command::write_filemarks_16, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, WriteFileMarks16_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    EXPECT_NO_THROW(Dispatch(tape, scsi_command::write_filemarks_16));
}

TEST(TapeTest, Locate10_simh)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    fstream file(filename);
    WriteGoodData(file);
    WriteGoodData(file);
    WriteFilemark(file);
    WriteFilemark(file);
    file.flush();

    // Locate object 2
    controller->SetCdbByte(6, 0x02);
    Dispatch(tape, scsi_command::locate_10);
    CheckPositions(tape, 1040, 2);

    // Locate object 0
    controller->SetCdbByte(6, 0x00);
    Dispatch(tape, scsi_command::locate_10);
    CheckPositions(tape, 0, 0);

    // Locate object 4
    controller->SetCdbByte(6, 0x04);
    Dispatch(tape, scsi_command::locate_10);
    CheckPositions(tape, 1048, 4);

    // BT
    controller->SetCdbByte(1, 0x04);
    Dispatch(tape, scsi_command::locate_10);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 1);
    Dispatch(tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, Locate10_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(6, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::locate_10));
    CheckPositions(tape, 512, 1);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 123);
    Dispatch(tape, scsi_command::locate_10, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(5, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::locate_10));
    CheckPositions(tape, 512, 1);
}

TEST(TapeTest, Locate16_simh)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    fstream file(filename);
    WriteGoodData(file);
    WriteGoodData(file);
    WriteFilemark(file);
    WriteFilemark(file);
    file.flush();

    // Locate object 2
    controller->SetCdbByte(11, 0x02);
    Dispatch(tape, scsi_command::locate_16);
    CheckPositions(tape, 1040, 2);

    // Locate object 0
    controller->SetCdbByte(11, 0x00);
    Dispatch(tape, scsi_command::locate_16);
    CheckPositions(tape, 0, 0);

    // Locate object 4
    controller->SetCdbByte(11, 0x04);
    Dispatch(tape, scsi_command::locate_16);
    CheckPositions(tape, 1048, 4);

    // BT
    controller->SetCdbByte(1, 0x04);
    Dispatch(tape, scsi_command::locate_16);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 1);
    Dispatch(tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(TapeTest, Locate16_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    controller->SetCdbByte(11, 1);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::locate_16));
    CheckPositions(tape, 512, 1);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 123);
    Dispatch(tape, scsi_command::locate_16, sense_key::illegal_request, asc::invalid_field_in_cdb);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(10, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::locate_16));
    CheckPositions(tape, 512, 1);
}

TEST(TapeTest, ReadPosition)
{
    auto [controller, tape] = CreateTape();

    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b11000000, controller->GetBuffer()[0]) << "BOP and EOP must be set";
}

TEST(TapeTest, FormatMedium_simh)
{
    auto [controller, tape] = CreateTape();

    CreateImageFile(*tape);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::format_medium));
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Write a filemark in order to advance the position
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, scsi_command::write_filemarks_6);
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    Dispatch(tape, scsi_command::format_medium, sense_key::illegal_request,
        asc::sequential_positioning_error);

    tape->SetProtected(true);
    Dispatch(tape, scsi_command::format_medium, sense_key::data_protect, asc::write_protected);
}

TEST(TapeTest, FormatMedium_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, scsi_command::format_medium, sense_key::illegal_request,
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

TEST(TapeTest, ModeSense6)
{
    auto [controller, tape] = CreateTape();

    // Drive must be ready in order to return all data
    tape->SetReady(true);

    controller->SetCdbByte(2, 0x00);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::mode_sense_6));
    EXPECT_EQ(8, controller->GetBuffer()[3]) << "Wrong block descriptor length";
    EXPECT_EQ(0U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block size";

    // Changeable values
    controller->SetCdbByte(2, 0x40);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::mode_sense_6));
    EXPECT_EQ(8, controller->GetBuffer()[3]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00ffffffU, GetInt32(controller->GetBuffer(), 8)) << "Wrong changeable block size";
}

TEST(TapeTest, ModeSense10)
{
    auto [controller, tape] = CreateTape();

    // Drive must be ready in order to return all data
    tape->SetReady(true);

    controller->SetCdbByte(2, 0x00);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::mode_sense_10));
    EXPECT_EQ(8, controller->GetBuffer()[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0U, GetInt32(controller->GetBuffer(), 12)) << "Wrong block size";

    // Changeable values
    controller->SetCdbByte(2, 0x40);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, scsi_command::mode_sense_10));
    EXPECT_EQ(8, controller->GetBuffer()[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00ffffffU, GetInt32(controller->GetBuffer(), 12)) << "Wrong changeable block size";
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
}

TEST(TapeTest, VerifyBlockSizeChange)
{
    MockTape tape;
    tape.SetBlockSize(512);

    EXPECT_EQ(0U, tape.VerifyBlockSizeChange(0, true));

    EXPECT_THAT([&] { tape.VerifyBlockSizeChange(0, false) ; }, Throws<scsi_exception>(AllOf(
        Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
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
