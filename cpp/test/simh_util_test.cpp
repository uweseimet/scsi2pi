//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/s2p_util.h"
#include "shared/simh_util.h"

using namespace s2p_util;
using namespace simh_util;
using enum SimhClass;

TEST(SimhUtilTest, ReadMetaData)
{
    stringstream stream;

    stream.write(to_const_char_ptr(ToLittleEndian( { TAPE_MARK_GOOD_DATA_RECORD, 0 })),
        META_DATA_SIZE);
    stream.write(to_const_char_ptr(ToLittleEndian( { TAPE_MARK_GOOD_DATA_RECORD, 0x1234567 })),
        META_DATA_SIZE);
    // end-of-data
    stream.write(to_const_char_ptr(ToLittleEndian( { PRIVATE_MARKER, 0b011 })), META_DATA_SIZE);
    stream.write(to_const_char_ptr(ToLittleEndian( { RESERVERD_MARKER, 0 })), META_DATA_SIZE);

    stream.seekg(0);

    SimhMetaData meta_data;
    EXPECT_TRUE(ReadMetaData(stream, meta_data));
    EXPECT_EQ(TAPE_MARK_GOOD_DATA_RECORD, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(stream, meta_data));
    EXPECT_EQ(TAPE_MARK_GOOD_DATA_RECORD, meta_data.cls);
    EXPECT_EQ(0x1234567U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(stream, meta_data));
    EXPECT_EQ(PRIVATE_MARKER, meta_data.cls);
    EXPECT_EQ(0b011U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(stream, meta_data));
    EXPECT_EQ(RESERVERD_MARKER, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(stream, meta_data));
    EXPECT_EQ(RESERVERD_MARKER, meta_data.cls);
    EXPECT_EQ(static_cast<uint32_t>(SimhMarker::END_OF_MEDIUM), meta_data.value);

    stream.setstate(ios::failbit);
    EXPECT_FALSE(ReadMetaData(stream, meta_data));
}

TEST(SimhUtilTest, IsRecord)
{
    EXPECT_TRUE(IsRecord( { TAPE_MARK_GOOD_DATA_RECORD, 1 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_1, 0 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_2, 0 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_3, 0 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_4, 0 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_5, 0 }));
    EXPECT_TRUE(IsRecord( { PRIVATE_DATA_RECORD_6, 0 }));
    EXPECT_TRUE(IsRecord( { BAD_DATA_RECORD, 1 }));
    EXPECT_TRUE(IsRecord( { RESERVED_DATA_RECORD_1, 0 }));
    EXPECT_TRUE(IsRecord( { RESERVED_DATA_RECORD_2, 0 }));
    EXPECT_TRUE(IsRecord( { RESERVED_DATA_RECORD_3, 0 }));
    EXPECT_TRUE(IsRecord( { RESERVED_DATA_RECORD_4, 0 }));
    EXPECT_TRUE(IsRecord( { RESERVED_DATA_RECORD_5, 0 }));
    EXPECT_TRUE(IsRecord( { TAPE_DESCRIPTION_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { TAPE_MARK_GOOD_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { BAD_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { PRIVATE_MARKER, 0 }));
    EXPECT_FALSE(IsRecord( { RESERVERD_MARKER, 0 }));
}

TEST(SimhUtilTest, Pad)
{
    EXPECT_EQ(0U, Pad(0));
    EXPECT_EQ(6U, Pad(6));
    EXPECT_EQ(8U, Pad(7));
}

TEST(SimhUtilTest, WriteFilemark)
{
    stringstream stream;

    EXPECT_TRUE(WriteFilemark(stream));

    array<uint8_t, 4> data;
    stream.seekg(0);
    stream.read(to_char_ptr(data), data.size());
    EXPECT_EQ(TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(0U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, WriteGoodData)
{
    stringstream stream;

    vector<uint8_t> data = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    EXPECT_TRUE(WriteGoodData(stream, data, 8));

    stream.seekg(0);
    data.resize(4);
    stream.read(to_char_ptr(data), data.size());
    EXPECT_EQ(TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(8U, FromLittleEndian(data).value);
    data.resize(8);
    stream.read(to_char_ptr(data), data.size());
    EXPECT_EQ(0x01, data[0]);
    EXPECT_EQ(0x02, data[1]);
    EXPECT_EQ(0x03, data[2]);
    EXPECT_EQ(0x04, data[3]);
    EXPECT_EQ(0x05, data[4]);
    EXPECT_EQ(0x06, data[5]);
    EXPECT_EQ(0x07, data[6]);
    EXPECT_EQ(0x08, data[7]);
    data.resize(4);
    stream.read(to_char_ptr(data), data.size());
    EXPECT_EQ(TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(8U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, FromLittleEndian)
{
    const array<uint8_t, META_DATA_SIZE> &data = { 0x01, 0x02, 0x03, 0x74 };
    EXPECT_EQ(PRIVATE_MARKER, FromLittleEndian(data).cls);
    EXPECT_EQ(0x04030201U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, ToLittleEndian)
{
    const auto &data = ToLittleEndian(SimhMetaData { PRIVATE_MARKER, 0x01020304 });
    EXPECT_EQ(0x04, data[0]);
    EXPECT_EQ(0x03, data[1]);
    EXPECT_EQ(0x02, data[2]);
    EXPECT_EQ(0x71, data[3]);
}
