//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/simh_util.h"
#include "test_shared.h"

using namespace simh_util;
using namespace testing;

TEST(SimhUtilTest, ReadHeader)
{
    const int FILE_SIZE = 12;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    const TapeFile &tapeFile = { file, FILE_SIZE };

    int64_t position = WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, 0 });
    position += WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, 0x1234567 });
    WriteHeader(tapeFile, { simh_class::reserved_marker, 0 });

    SimhHeader header;

    file.seekg(0, ios::beg);
    position = ReadHeader(tapeFile, header);
    EXPECT_EQ(HEADER_SIZE, position);
    EXPECT_EQ(simh_class::tape_mark_good_data_record, header.cls);
    EXPECT_EQ(0U, header.value);

    position += ReadHeader(tapeFile, header);
    EXPECT_EQ(2 * HEADER_SIZE, position);
    EXPECT_EQ(simh_class::tape_mark_good_data_record, header.cls);
    EXPECT_EQ(0x1234567U, header.value);

    position += ReadHeader(tapeFile, header);
    EXPECT_EQ(3 * HEADER_SIZE, position);
    EXPECT_EQ(simh_class::reserved_marker, header.cls);
    EXPECT_EQ(0U, header.value);

    EXPECT_EQ(0, ReadHeader(tapeFile, header));
    EXPECT_EQ(simh_class::reserved_marker, header.cls);
    EXPECT_EQ(static_cast<uint32_t>(simh_marker::end_of_medium), header.value);
}

TEST(SimhUtilTest, WriteHeader)
{
    const int FILE_SIZE = 8;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::out | ios::binary);
    assert(file.good());

    const TapeFile &tapeFile = { file, FILE_SIZE };

    int64_t position = WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, 1 });
    EXPECT_EQ(HEADER_SIZE, position);

    position += WriteHeader(tapeFile, { simh_class::reserved_marker, 0 });
    EXPECT_EQ(2 * HEADER_SIZE, position);

    EXPECT_EQ(-1, WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, 0 }));
}

TEST(SimhUtilTest, ReadRecord)
{
    vector<uint8_t> buf = { 0x01, 0x02, 0x03, 0x04 };
    const int file_size = buf.size() + HEADER_SIZE;

    fstream file;
    file.open(CreateTempFile(file_size), ios::in | ios::out | ios::binary);
    assert(file.good());

    const TapeFile &tapeFile = { file, file_size };

    WriteRecord(tapeFile, buf, static_cast<int>(buf.size()));
    ranges::fill(buf, 0);
    file.seekg(0, ios::beg);
    EXPECT_EQ(file_size, ReadRecord(tapeFile, buf, static_cast<int>(buf.size())));
    EXPECT_EQ(0x01, buf[0]);
    EXPECT_EQ(0x02, buf[1]);
    EXPECT_EQ(0x03, buf[2]);
    EXPECT_EQ(0x04, buf[3]);

    file.seekg(file_size, ios::beg);
    EXPECT_EQ(READ_ERROR, ReadRecord(tapeFile, buf, static_cast<int>(buf.size())));
}

TEST(SimhUtilTest, WriteRecord)
{
    const int FILE_SIZE = 8;
    fstream file;

    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    array<uint8_t, 4> buf = { };

    EXPECT_EQ(static_cast<int>(HEADER_SIZE + buf.size()),
        WriteRecord( { file, FILE_SIZE }, buf, static_cast<int>(buf.size())));
    file.seekg(-HEADER_SIZE, ios::cur);
    uint32_t trailing_length;
    file.read((char*)&trailing_length, HEADER_SIZE);
    EXPECT_EQ(4U, trailing_length);

    file.seekg(0, ios::beg);
    file.seekp(0, ios::beg);
    EXPECT_EQ(HEADER_SIZE + 2, WriteRecord( { file, FILE_SIZE }, buf, 1));
    file.seekg(-HEADER_SIZE, ios::cur);
    file.read((char*)&trailing_length, HEADER_SIZE);
    EXPECT_EQ(1U, trailing_length);

    EXPECT_EQ(OVERFLOW_ERROR, WriteRecord( { file, FILE_SIZE }, buf, 1));
}

TEST(SimhUtilTest, MoveBack)
{
    const int FILE_SIZE = 32;
    const int RECORD_LENGTH_ODD = 1;
    const int RECORD_LENGTH_EVEN = 2;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    const TapeFile &tapeFile = { file, FILE_SIZE };
    const array<uint8_t, 2> &buf = { 0xaa, 0xaa };

    int64_t position = WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, RECORD_LENGTH_ODD });
    position += WriteRecord(tapeFile, buf, RECORD_LENGTH_ODD);
    position += WriteHeader(tapeFile, { simh_class::tape_mark_good_data_record, RECORD_LENGTH_EVEN });
    position += WriteRecord(tapeFile, buf, RECORD_LENGTH_EVEN);
    position += WriteHeader(tapeFile, { simh_class::reserved_marker, static_cast<int>(simh_marker::erase_gap) });
    position += WriteHeader(tapeFile, { simh_class::reserved_marker, static_cast<int>(simh_marker::end_of_medium) });
    EXPECT_EQ(28, position);

    file.seekg(position, ios::beg);

    position = MoveBack(file);
    EXPECT_EQ(24, position);

    position = MoveBack(file);
    EXPECT_EQ(20, position);

    position = MoveBack(file);
    EXPECT_EQ(10, position);

    position = MoveBack(file);
    EXPECT_EQ(0, position);

    position = MoveBack(file);
    EXPECT_EQ(-1, position);
}

TEST(SimhUtilTest, Pad)
{
    EXPECT_EQ(2, Pad(2));
    EXPECT_EQ(6, Pad(5));
}

TEST(SimhUtilTest, FromLittleEndian)
{
    const array<uint8_t, HEADER_SIZE> &data = { 0x01, 0x02, 0x03, 0x04 };
    EXPECT_EQ(0x04030201, FromLittleEndian(data));
}

TEST(SimhUtilTest, ToLittleEndian)
{
    const auto &data = ToLittleEndian(0x01020304);
    EXPECT_EQ(0x04, data[0]);
    EXPECT_EQ(0x03, data[1]);
    EXPECT_EQ(0x02, data[2]);
    EXPECT_EQ(0x01, data[3]);
}
