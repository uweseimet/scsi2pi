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
    file.write((const char*)ToLittleEndian((static_cast<int>(simh_class::tape_mark_good_data_record) << 28)).data(),
        HEADER_SIZE);
    file.write(
        (const char*)ToLittleEndian((static_cast<int>(simh_class::tape_mark_good_data_record) << 28) + 0x1234567).data(),
        HEADER_SIZE);
    // end-of-data
    file.write((const char*)ToLittleEndian((static_cast<int>(simh_class::private_marker) << 28) + 0b11).data(),
        HEADER_SIZE);
    file.write((const char*)ToLittleEndian((static_cast<int>(simh_class::reserved_marker) << 28)).data(),
        HEADER_SIZE);

    SimhHeader header;

    file.seekg(0, ios::beg);
    int64_t position = ReadHeader(file, header);
    EXPECT_EQ(HEADER_SIZE, position);
    EXPECT_EQ(simh_class::tape_mark_good_data_record, header.cls);
    EXPECT_EQ(0U, header.value);

    position += ReadHeader(file, header);
    EXPECT_EQ(2 * HEADER_SIZE, position);
    EXPECT_EQ(simh_class::tape_mark_good_data_record, header.cls);
    EXPECT_EQ(0x1234567U, header.value);

    position += ReadHeader(file, header);
    EXPECT_EQ(3 * HEADER_SIZE, position);
    EXPECT_EQ(simh_class::private_marker, header.cls);
    EXPECT_EQ(0b011U, header.value);

    position += ReadHeader(file, header);
    EXPECT_EQ(4 * HEADER_SIZE, position);
    EXPECT_EQ(simh_class::reserved_marker, header.cls);
    EXPECT_EQ(0U, header.value);

    EXPECT_EQ(0, ReadHeader(file, header));
    EXPECT_EQ(simh_class::reserved_marker, header.cls);
    EXPECT_EQ(static_cast<uint32_t>(simh_marker::end_of_medium), header.value);
}

TEST(SimhUtilTest, GetPadding)
{
    EXPECT_EQ(0, GetPadding(0));
    EXPECT_EQ(0, GetPadding(6));
    EXPECT_EQ(1, GetPadding(7));
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
