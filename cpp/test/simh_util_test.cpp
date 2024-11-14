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
    const int FILE_SIZE = 16;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    int64_t position = 0;

    position += WriteHeader(file, position, FILE_SIZE, simh_class::tape_mark_good_data_record, 0);
    position += WriteHeader(file, position, FILE_SIZE, simh_class::tape_mark_good_data_record, 0x1234567);
    WriteHeader(file, position, FILE_SIZE, simh_class::reserved_marker, 0);

    position = 0;

    const auto [cls_filemark, value_filemark] = ReadHeader(file, position, FILE_SIZE);
    EXPECT_EQ(HEADER_SIZE, position);
    EXPECT_EQ(cls_filemark, simh_class::tape_mark_good_data_record);
    EXPECT_EQ(value_filemark, 0);

    const auto [cls_record, value_record] = ReadHeader(file, position, FILE_SIZE);
    EXPECT_EQ(2 * HEADER_SIZE, position);
    EXPECT_EQ(cls_record, simh_class::tape_mark_good_data_record);
    EXPECT_EQ(value_record, 0x1234567);

    const auto [cls_end_of_data, value_end_of_data] = ReadHeader(file, position, FILE_SIZE);
    EXPECT_EQ(3 * HEADER_SIZE, position);
    EXPECT_EQ(cls_end_of_data, simh_class::reserved_marker);
    EXPECT_EQ(value_end_of_data, 0);

    position = FILE_SIZE;
    const auto [cls_end_of_file, value_end_of_file] = ReadHeader(file, position, FILE_SIZE);
    EXPECT_EQ(FILE_SIZE, position);
    EXPECT_EQ(cls_end_of_file, simh_class::reserved_marker);
    EXPECT_EQ(value_end_of_file, static_cast<int>(simh_marker::end_of_medium));
}

TEST(SimhUtilTest, WriteHeader)
{
    const int FILE_SIZE = 8;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::out | ios::binary);
    assert(file.good());

    EXPECT_EQ(-1, WriteHeader(file, 512, FILE_SIZE, simh_class::tape_mark_good_data_record, 0));

    int64_t position = 0;

    position += WriteHeader(file, position, FILE_SIZE, simh_class::tape_mark_good_data_record, 1);
    EXPECT_EQ(HEADER_SIZE, position);

    position += WriteHeader(file, position, FILE_SIZE, simh_class::reserved_marker, 0);
    EXPECT_EQ(2 * HEADER_SIZE, position);
}

TEST(SimhUtilTest, ReadRecord)
{
    vector<uint8_t> buf = { 0x01, 0x02, 0x03, 0x04 };

    fstream file;
    file.open(CreateTempFile(buf.size() + HEADER_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    EXPECT_EQ(-1, ReadRecord(file, buf.size() + HEADER_SIZE, buf, static_cast<int>(buf.size())));

    WriteRecord(file, 0, buf.size() + HEADER_SIZE, buf, static_cast<int>(buf.size()));
    ranges::fill(buf, 0);
    EXPECT_EQ(buf.size(), ReadRecord(file, 0, buf, static_cast<int>(buf.size())));
    EXPECT_EQ(0x01, buf[0]);
    EXPECT_EQ(0x02, buf[1]);
    EXPECT_EQ(0x03, buf[2]);
    EXPECT_EQ(0x04, buf[3]);
}

TEST(SimhUtilTest, WriteRecord)
{
    const int FILE_SIZE = 8;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    array<uint8_t, 4> buf = { };

    EXPECT_EQ(HEADER_SIZE + buf.size(), WriteRecord(file, 0, FILE_SIZE, buf, static_cast<int>(buf.size())));
    file.seekg(-HEADER_SIZE, ios::cur);
    uint32_t trailing_length;
    file.read((char*)&trailing_length, HEADER_SIZE);
    EXPECT_EQ(4, trailing_length);

    file.seekg(0, ios::beg);
    file.seekp(0, ios::beg);
    EXPECT_EQ(HEADER_SIZE + 2, WriteRecord(file, 0, FILE_SIZE, buf, 1));
    file.seekg(-HEADER_SIZE, ios::cur);
    file.read((char*)&trailing_length, HEADER_SIZE);
    EXPECT_EQ(1, trailing_length);
}

TEST(SimhUtilTest, MoveBack)
{
    const int FILE_SIZE = 32;
    const int RECORD_LENGTH_ODD = 1;
    const int RECORD_LENGTH_EVEN = 2;

    fstream file;
    file.open(CreateTempFile(FILE_SIZE), ios::in | ios::out | ios::binary);
    assert(file.good());

    const array<uint8_t, 2> &buf = { 0xaa, 0xaa };
    int64_t position = 0;

    position += WriteHeader(file, position, FILE_SIZE, simh_class::tape_mark_good_data_record, RECORD_LENGTH_ODD);
    position += WriteRecord(file, position, FILE_SIZE, buf, RECORD_LENGTH_ODD);
    position += WriteHeader(file, position, FILE_SIZE, simh_class::tape_mark_good_data_record, RECORD_LENGTH_EVEN);
    position += WriteRecord(file, position, FILE_SIZE, buf, RECORD_LENGTH_EVEN);
    position += WriteHeader(file, position, FILE_SIZE, simh_class::reserved_marker,
        static_cast<int>(simh_marker::erase_gap));
    position += WriteHeader(file, position, FILE_SIZE, simh_class::reserved_marker,
        static_cast<int>(simh_marker::end_of_medium));
    EXPECT_EQ(28, position);

    position = MoveBack(file, position);
    EXPECT_EQ(24, position);
    position = MoveBack(file, position);
    EXPECT_EQ(20, position);
    position = MoveBack(file, position);
    EXPECT_EQ(10, position);
    position = MoveBack(file, position);
    EXPECT_EQ(0, position);
    position = MoveBack(file, position);
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
