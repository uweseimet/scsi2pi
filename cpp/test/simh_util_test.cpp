//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/simh_util.h"
#include "test_shared.h"

using namespace simh_util;
using namespace testing;

TEST(SimhUtilTest, ReadMetaData)
{
    fstream file;
    file.open(CreateTempFile(), ios::in | ios::out | ios::binary);
    file.write(reinterpret_cast<const char*>(ToLittleEndian( { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 0 }).data()),
        META_DATA_SIZE);
    file.write(
        reinterpret_cast<const char*>(ToLittleEndian( { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 0x1234567 }).data()),
        META_DATA_SIZE);
    // end-of-data
    file.write(reinterpret_cast<const char*>(ToLittleEndian( { SimhClass::PRIVATE_MARKER, 0b011 }).data()),
        META_DATA_SIZE);
    file.write(reinterpret_cast<const char*>(ToLittleEndian( { SimhClass::RESERVERD_MARKER, 0 }).data()),
        META_DATA_SIZE);
    file.flush();

    file.seekg(0);

    SimhMetaData meta_data;
    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, meta_data.cls);
    EXPECT_EQ(0x1234567U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(SimhClass::PRIVATE_MARKER, meta_data.cls);
    EXPECT_EQ(0b011U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(SimhClass::RESERVERD_MARKER, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(SimhClass::RESERVERD_MARKER, meta_data.cls);
    EXPECT_EQ(static_cast<uint32_t>(SimhMarker::END_OF_MEDIUM), meta_data.value);
}

TEST(SimhUtilTest, IsRecord)
{
    EXPECT_TRUE(IsRecord( { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 1 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_1, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_2, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_3, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_4, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_5, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::PRIVATE_DATA_RECORD_6, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::BAD_DATA_RECORD, 1 }));
    EXPECT_TRUE(IsRecord( { SimhClass::RESERVED_DATA_RECORD_1, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::RESERVED_DATA_RECORD_2, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::RESERVED_DATA_RECORD_3, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::RESERVED_DATA_RECORD_4, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::RESERVED_DATA_RECORD_5, 0 }));
    EXPECT_TRUE(IsRecord( { SimhClass::TAPE_DESCRIPTION_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { SimhClass::BAD_DATA_RECORD, 0 }));
    EXPECT_FALSE(IsRecord( { SimhClass::PRIVATE_MARKER, 0 }));
    EXPECT_FALSE(IsRecord( { SimhClass::RESERVERD_MARKER, 0 }));
}

TEST(SimhUtilTest, Pad)
{
    EXPECT_EQ(0U, Pad(0));
    EXPECT_EQ(6U, Pad(6));
    EXPECT_EQ(8U, Pad(7));
}

TEST(SimhUtilTest, WriteFilemark)
{
    const string &filename = CreateTempFile();
    fstream file(filename);

    EXPECT_TRUE(WriteFilemark(file));
    file.flush();

    EXPECT_EQ(4U, file_size(filename));
    array<uint8_t, 4> data;
    file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), data.size());
    EXPECT_EQ(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(0U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, WriteGoodData)
{
    const string &filename = CreateTempFile();
    fstream file(filename);

    vector<uint8_t> data = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    EXPECT_TRUE(WriteGoodData(file, data, 8));
    file.flush();

    EXPECT_EQ(16U, file_size(filename));
    file.seekg(0);
    data.resize(4);
        file.read(reinterpret_cast<char*>(data.data()), data.size());
    EXPECT_EQ(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(8U, FromLittleEndian(data).value);
    data.resize(8);
    file.read(reinterpret_cast<char*>(data.data()), data.size());
    EXPECT_EQ(0x01, data[0]);
    EXPECT_EQ(0x02, data[1]);
    EXPECT_EQ(0x03, data[2]);
    EXPECT_EQ(0x04, data[3]);
    EXPECT_EQ(0x05, data[4]);
    EXPECT_EQ(0x06, data[5]);
    EXPECT_EQ(0x07, data[6]);
    EXPECT_EQ(0x08, data[7]);
    data.resize(4);
    file.read(reinterpret_cast<char*>(data.data()), data.size());
    EXPECT_EQ(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, FromLittleEndian(data).cls);
    EXPECT_EQ(8U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, FromLittleEndian)
{
    const array<uint8_t, META_DATA_SIZE> &data = { 0x01, 0x02, 0x03, 0x74 };
    EXPECT_EQ(SimhClass::PRIVATE_MARKER, FromLittleEndian(data).cls);
    EXPECT_EQ(0x04030201U, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, ToLittleEndian)
{
    const auto &data = ToLittleEndian(SimhMetaData { SimhClass::PRIVATE_MARKER, 0x01020304 });
    EXPECT_EQ(0x04, data[0]);
    EXPECT_EQ(0x03, data[1]);
    EXPECT_EQ(0x02, data[2]);
    EXPECT_EQ(0x71, data[3]);
}
