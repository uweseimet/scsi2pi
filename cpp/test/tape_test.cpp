//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

using namespace memory_util;

static void CheckPosition(const AbstractController &controller, shared_ptr<PrimaryDevice> tape, uint32_t position)
{
    fill_n(controller.GetBuffer().begin(), 12, 0xff);
    Dispatch(tape, ScsiCommand::READ_POSITION);

    if (position != GetInt32(controller.GetBuffer(), 4) || position != GetInt32(controller.GetBuffer(), 8)) {
        EXPECT_EQ(position, GetInt32(controller.GetBuffer(), 4));
    }
}

static void CheckPositions(shared_ptr<PrimaryDevice> tape, uint32_t position, uint32_t object_location)
{
    const auto c = static_cast<MockAbstractController*>(tape->GetController());
    c->ResetCdb();
    c->SetCdbByte(1, 0x01);
    CheckPosition(*c, tape, position);
    c->SetCdbByte(1, 0x00);
    CheckPosition(*c, tape, object_location);
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
    EXPECT_EQ("", tape->Init());
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
    Dispatch(tape, ScsiCommand::REWIND);
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
    TestShared::Inquiry(SCTP, DeviceType::SEQUENTIAL_ACCESS, ScsiLevel::SCSI_2, "SCSI2Pi SCSI TAPE       ", 0x1f,
        true);
}

TEST(TapeTest, ValidateFile)
{
    MockTape tape;

    EXPECT_THROW(tape.ValidateFile(), IoException)<< "Invalid block count";

    tape.SetBlockCount(1);
    EXPECT_THROW(tape.ValidateFile(), IoException)<< "Missing filename";

    const auto &filename = CreateTempFile(1);
    tape.SetFilename(filename.string());
    EXPECT_NO_THROW(tape.ValidateFile());
}

TEST(TapeTest, Open)
{
    Tape tape(0);
    tape.SetParams( { });

    EXPECT_THROW(tape.Open(), IoException);

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

    Dispatch(tape, ScsiCommand::READ_6, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::READ_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // Non-fixed, 0 bytes
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));

    // Fixed, 0 blocks
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
    CheckPositions(tape, 0, 0);

    // Fixed and SILI, 0 blocks
    controller->SetCdbByte(1, 0x03);
    Dispatch(tape, ScsiCommand::READ_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
    CheckPositions(tape, 540, 2);

    // Fixed, 1 block, bad data recovered
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
    CheckPositions(tape, 1060, 3);

    // Fixed, 1 block, bad data
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, ScsiCommand::READ_6, SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
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
    Dispatch(tape, ScsiCommand::READ_6, SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);

    file.seekp(0);
    WriteFilemark(file);
    file.flush();

    Rewind(tape);

    // Non-fixed, 90 byte
    controller->SetCdbByte(4, 90);
    Dispatch(tape, ScsiCommand::READ_6, SenseKey::NO_SENSE, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(90U, GetInt32(buf, 3));

    Rewind(tape);

    // Fixed, 1 block
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, ScsiCommand::READ_6, SenseKey::NO_SENSE, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(0U, GetInt32(buf, 3));
}

TEST(TapeTest, Read6_BlockSizeMismatch)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::READ_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    fstream file(filename);
    WriteGoodData(file, 256);
    file.flush();

    const auto &buf = controller->GetBuffer();

    // Non-fixed, 1 byte (less than block size)
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
    EXPECT_EQ(1, controller->GetCurrentLength()) << "Wrong actual length";
    RequestSense(controller, tape);
    EXPECT_FALSE(buf[0] & 0x80) << "VALID must not be set";
    EXPECT_FALSE(buf[2] & 0x20) << "ILI must not be set";
    CheckPositions(tape, 264, 1);

    Rewind(tape);

    // Non-fixed, 1024 bytes (more than block size)
    controller->SetCdbByte(3, 0x04);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_6));
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

    Dispatch(tape, ScsiCommand::READ_6, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::READ_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // Partition 1
    controller->SetCdbByte(3, 1);
    Dispatch(tape, ScsiCommand::READ_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    CheckPositions(tape, 0, 0);

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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_16));
    CheckPositions(tape, 1560, 3);

    // Fixed, non-existing block 10
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 10);
    controller->SetCdbByte(14, 1);
    Dispatch(tape, ScsiCommand::READ_16, SenseKey::NO_SENSE, Asc::LOCATE_OPERATION_FAILURE);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
}

TEST(TapeTest, Write6)
{
    auto [controller, tape] = CreateTape();

    Dispatch(tape, ScsiCommand::WRITE_6, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::WRITE_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // Non-fixed, 0 bytes
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_6));
    CheckPositions(tape, 0, 0);

    ifstream file(filename);

    // Non-fixed, 2 bytes
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::WRITE_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 2);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 2));
    CheckMetaData(file, { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 2 });
    CheckPositions(tape, 10, 1);

    Rewind(tape);
    file.seekg(0);

    // Non-fixed, 1 byte
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::WRITE_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 1));
    CheckMetaData(file, { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 1 });
    CheckPositions(tape, 10, 1);

    Rewind(tape);
    file.seekg(0);

    // Non-fixed, 512 bytes
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::WRITE_6));
    controller->SetCdbByte(1, 0x00);
    controller->SetCdbByte(3, 2);
    controller->SetCdbByte(4, 0);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    CheckMetaData(file, { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 512 });
    CheckPositions(tape, 520, 1);

    Rewind(tape);
    file.seekg(0);

    // Fixed, 1 block
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::WRITE_6));
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_6));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    CheckMetaData(file, { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 512 });
    CheckPositions(tape, 520, 1);
}

TEST(TapeTest, Write16)
{
    auto [controller, tape] = CreateTape();

    Dispatch(tape, ScsiCommand::WRITE_16, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::WRITE_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // FCS/LCS
    controller->SetCdbByte(1, 0b1100);
    Dispatch(tape, ScsiCommand::WRITE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    CheckPositions(tape, 0, 0);

    // Partition 1
    controller->SetCdbByte(3, 1);
    Dispatch(tape, ScsiCommand::WRITE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    CheckPositions(tape, 0, 0);

    fstream file(filename);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    WriteGoodData(file, 512);
    file.flush();

    // Fixed, block 2
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 2);
    controller->SetCdbByte(14, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_16));
    EXPECT_NO_THROW(tape->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 512));
    file.seekg(1040);
    CheckMetaData(file, { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 512 });
    CheckPositions(tape, 1560, 3);

    // Fixed, non-existing block 10
    controller->SetCdbByte(1, 0x01);
    controller->SetCdbByte(11, 10);
    controller->SetCdbByte(14, 1);
    Dispatch(tape, ScsiCommand::WRITE_16, SenseKey::NO_SENSE, Asc::LOCATE_OPERATION_FAILURE);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
}

TEST(TapeTest, Erase6_simh)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 4567);

    Dispatch(tape, ScsiCommand::ERASE_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    tape->SetProtected(true);
    Dispatch(tape, ScsiCommand::ERASE_6, SenseKey::DATA_PROTECT, Asc::WRITE_PROTECTED);

    tape->SetProtected(false);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::ERASE_6));
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "EOP must be set";

    Rewind(tape);

    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6);
    controller->SetCdbByte(4, 0x00);
    // Long
    controller->SetCdbByte(1, 0x01);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::ERASE_6));
    controller->SetCdbByte(1, 0x00);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";
}

TEST(TapeTest, Erase6_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::ERASE_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    Dispatch(tape, ScsiCommand::ERASE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
}

TEST(TapeTest, ReadBlockLimits)
{
    auto [controller, tape] = CreateTape();

    CreateImageFile(*tape);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::READ_BLOCK_LIMITS));
    EXPECT_EQ(0x02fffffcU, GetInt32(controller->GetBuffer(), 0));
    EXPECT_EQ(4, GetInt16(controller->GetBuffer(), 4));
}

TEST(TapeTest, Rewind)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 600);

    Dispatch(tape, ScsiCommand::REWIND, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    Rewind(tape);
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Set filemark in order to advance the tape position
    controller->SetCdbByte(4, 0x01);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6);
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

    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // Invalid object type
    controller->SetCdbByte(1, 0b111);
    controller->SetCdbByte(2, 1);
    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);


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
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 4, 1);

    // Space over 3 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 3);
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 16, 4);

    // Reverse-space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 12, 3);

    // Reverse-space over 2 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 4, 1);

    // Try to space over 10 filemarks, hitting end-of-data
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 10);
    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::BLANK_CHECK);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, controller->GetBuffer()[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(5U, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 24, 6);

    Rewind(tape);

    // Search for end-of-data, the count must be ignored
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(4, 255);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 24, 6);

    Rewind(tape);

    // Space over 1 filemark, then reverse-space over more filemarks than available
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 4, 1);
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0x00);
    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::NO_SENSE, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
    RequestSense(controller, tape);
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0b01000000, buf[2]) << "EOM must be set";
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(Ascq::BEGINNING_OF_PARTITION_MEDIUM_DETECTED, static_cast<Ascq>(buf[13]))
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 520, 1);

    // Space over 3 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 3);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 1564, 4);

    // Reverse-space over 2 blocks
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfe);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 524, 2);

    // Try to space over 6 blocks, in order to hit the filemark
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 6);
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 2630, 8);
    RequestSense(controller, tape);
    EXPECT_EQ(0x80, buf[0] & 0x80) << "VALID must be set";
    EXPECT_EQ(2U, GetInt32(buf, 3));

    // Reverse-space over 1 filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 2626, 7);

    // Try to reverse-space over non-existing filemark
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(tape, ScsiCommand::SPACE_6);
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
    Dispatch(tape, ScsiCommand::SPACE_6);
    CheckPositions(tape, 524, 2);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 1044, 3);

    // Space over 1 block
    controller->SetCdbByte(1, 0b000);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::BLANK_CHECK);
    RequestSense(controller, tape);
    EXPECT_NE(Ascq::END_OF_DATA_DETECTED, static_cast<Ascq>(controller->GetBuffer()[13]));
    EXPECT_TRUE(controller->GetBuffer()[0] & 0x80);
    EXPECT_EQ(1U, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 1044, 3);

    // Reverse-space over 1 block
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    RequestSense(controller, tape);
    EXPECT_EQ(0U, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 524, 2);

    // Reverse-space over 6 blocks, which will immediately hit the filemark
    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xfa);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    RequestSense(controller, tape);
    EXPECT_EQ(0xfffffffaU, GetInt32(controller->GetBuffer(), 3));
    CheckPositions(tape, 520, 1);

    Rewind(tape);

    // Space for end-of-data
    controller->SetCdbByte(1, 0b011);
    controller->SetCdbByte(4, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::SPACE_6));
    CheckPositions(tape, 1044, 3);
}

TEST(TapeTest, Space6_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    Dispatch(tape, ScsiCommand::SPACE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
}

TEST(TapeTest, WriteFileMarks6_simh)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 1024);

    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // Setmarks are not supported
    controller->SetCdbByte(1, 0b010);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    // 0 filemarks
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6));

    // 100 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6));
    CheckPositions(tape, 400, 0);

    // 100 filemarks
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 100);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6, SenseKey::VOLUME_OVERFLOW);
    CheckPositions(tape, 512, 0);

    tape->SetProtected(true);
    controller->SetCdbByte(1, 0b001);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6, SenseKey::DATA_PROTECT, Asc::WRITE_PROTECTED);
}

TEST(TapeTest, WriteFileMarks6_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6));
}

TEST(TapeTest, WriteFileMarks16_simh)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 1024);

    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // 0 filemarks
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16));

    // 100 filemarks
    controller->SetCdbByte(14, 100);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16));
    CheckPositions(tape, 400, 0);

    // 100 filemarks
    controller->SetCdbByte(14, 100);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16, SenseKey::VOLUME_OVERFLOW);
    CheckPositions(tape, 512, 0);

    tape->SetProtected(true);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16, SenseKey::DATA_PROTECT, Asc::WRITE_PROTECTED);
}

TEST(TapeTest, WriteFileMarks16_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_16));
}

TEST(TapeTest, Locate10_simh)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    fstream file(filename);
    WriteGoodData(file);
    WriteGoodData(file);
    WriteFilemark(file);
    WriteFilemark(file);
    file.flush();

    // Locate object 2
    controller->SetCdbByte(6, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_10);
    CheckPositions(tape, 1040, 2);

    // Locate object 0
    controller->SetCdbByte(6, 0x00);
    Dispatch(tape, ScsiCommand::LOCATE_10);
    CheckPositions(tape, 0, 0);

    // Locate object 4
    controller->SetCdbByte(6, 0x04);
    Dispatch(tape, ScsiCommand::LOCATE_10);
    CheckPositions(tape, 1048, 4);

    // BT
    controller->SetCdbByte(1, 0x04);
    Dispatch(tape, ScsiCommand::LOCATE_10);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 1);
    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
}

TEST(TapeTest, Locate10_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    controller->SetCdbByte(6, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::LOCATE_10));
    CheckPositions(tape, 512, 1);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(6, 123);
    Dispatch(tape, ScsiCommand::LOCATE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(5, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::LOCATE_10));
    CheckPositions(tape, 512, 1);
}

TEST(TapeTest, Locate16_simh)
{
    auto [controller, tape] = CreateTape();
    const string &filename = CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    fstream file(filename);
    WriteGoodData(file);
    WriteGoodData(file);
    WriteFilemark(file);
    WriteFilemark(file);
    file.flush();

    // Locate object 2
    controller->SetCdbByte(11, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_16);
    CheckPositions(tape, 1040, 2);

    // Locate object 0
    controller->SetCdbByte(11, 0x00);
    Dispatch(tape, ScsiCommand::LOCATE_16);
    CheckPositions(tape, 0, 0);

    // Locate object 4
    controller->SetCdbByte(11, 0x04);
    Dispatch(tape, ScsiCommand::LOCATE_16);
    CheckPositions(tape, 1048, 4);

    // BT
    controller->SetCdbByte(1, 0x04);
    Dispatch(tape, ScsiCommand::LOCATE_16);
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 1);
    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
}

TEST(TapeTest, Locate16_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    // CP is not supported
    controller->SetCdbByte(1, 0x02);
    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    controller->SetCdbByte(11, 1);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::LOCATE_16));
    CheckPositions(tape, 512, 1);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(11, 123);
    Dispatch(tape, ScsiCommand::LOCATE_16, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    // BT
    controller->SetCdbByte(1, 0x04);
    controller->SetCdbByte(10, 0x02);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::LOCATE_16));
    CheckPositions(tape, 512, 1);
}

TEST(TapeTest, ReadPosition)
{
    auto [controller, tape] = CreateTape();

    tape->SetReady(true);

    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b11000000, controller->GetBuffer()[0]) << "BOP and EOP must be set";
}

TEST(TapeTest, FormatMedium_simh)
{
    auto [controller, tape] = CreateTape();

    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    CreateImageFile(*tape);

    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::FORMAT_MEDIUM));
    CheckPositions(tape, 0, 0);
    EXPECT_EQ(0b10000000, controller->GetBuffer()[0]) << "BOP must be set";

    // Write a filemark in order to advance the position
    controller->SetCdbByte(1, 0b001);
    controller->SetCdbByte(4, 1);
    Dispatch(tape, ScsiCommand::WRITE_FILEMARKS_6);
    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(4, 0);
    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::ILLEGAL_REQUEST,
        Asc::SEQUENTIAL_POSITIONING_ERROR);

    tape->SetProtected(true);
    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::DATA_PROTECT, Asc::WRITE_PROTECTED);
}

TEST(TapeTest, FormatMedium_tar)
{
    auto [controller, tape] = CreateTape();
    CreateImageFile(*tape, 512, "tar");

    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);

    Dispatch(tape, ScsiCommand::FORMAT_MEDIUM, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::MODE_SENSE_6));
    EXPECT_EQ(8, controller->GetBuffer()[3]) << "Wrong block descriptor length";
    EXPECT_EQ(0U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block size";

    // Changeable values
    controller->SetCdbByte(2, 0x40);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::MODE_SENSE_6));
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
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::MODE_SENSE_10));
    EXPECT_EQ(8, controller->GetBuffer()[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0U, GetInt32(controller->GetBuffer(), 12)) << "Wrong block size";

    // Changeable values
    controller->SetCdbByte(2, 0x40);
    // ALLOCATION LENGTH, block descriptor only
    controller->SetCdbByte(4, 12);
    EXPECT_NO_THROW(Dispatch(tape, ScsiCommand::MODE_SENSE_10));
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

    EXPECT_THAT([&] { tape.VerifyBlockSizeChange(0, false) ; }, Throws<ScsiException>(AllOf(
        Property(&ScsiException::GetSenseKey, SenseKey::ILLEGAL_REQUEST),
        Property(&ScsiException::GetAsc, Asc::INVALID_FIELD_IN_PARAMETER_LIST))));
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
