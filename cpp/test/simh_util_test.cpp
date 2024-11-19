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

TEST(SimhUtilTest, ReadMetaData)
{
    fstream file;
    file.open(CreateTempName(), ios::in | ios::out | ios::binary);
    file.write((const char*)ToLittleEndian( { simh_class::tape_mark_good_data_record, 0 }).data(), META_DATA_SIZE);
    file.write((const char*)ToLittleEndian( { simh_class::tape_mark_good_data_record, 0x1234567 }).data(),
        META_DATA_SIZE);
    // end-of-data
    file.write((const char*)ToLittleEndian( { simh_class::private_marker, 0b011 }).data(), META_DATA_SIZE);
    file.write((const char*)ToLittleEndian( { simh_class::reserved_marker, 0 }).data(), META_DATA_SIZE);

    file.seekg(0, ios::beg);

    SimhMetaData meta_data;
    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(simh_class::tape_mark_good_data_record, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(simh_class::tape_mark_good_data_record, meta_data.cls);
    EXPECT_EQ(0x1234567U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(simh_class::private_marker, meta_data.cls);
    EXPECT_EQ(0b011U, meta_data.value);

    EXPECT_TRUE(ReadMetaData(file, meta_data));
    EXPECT_EQ(simh_class::reserved_marker, meta_data.cls);
    EXPECT_EQ(0U, meta_data.value);
}

TEST(SimhUtilTest, IsRecord)
{
    EXPECT_TRUE(IsRecord( { simh_class::tape_mark_good_data_record, 1 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_1, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_2, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_3, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_4, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_5, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::private_data_record_6, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::bad_data_record, 1 }));
    EXPECT_TRUE(IsRecord( { simh_class::reserved_data_record_1, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::reserved_data_record_2, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::reserved_data_record_3, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::reserved_data_record_4, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::reserved_data_record_5, 0 }));
    EXPECT_TRUE(IsRecord( { simh_class::tape_description_data_record, 0 }));
    EXPECT_FALSE(IsRecord( { simh_class::tape_mark_good_data_record, 0 }));
    EXPECT_FALSE(IsRecord( { simh_class::bad_data_record, 0 }));
    EXPECT_FALSE(IsRecord( { simh_class::private_marker, 0 }));
    EXPECT_FALSE(IsRecord( { simh_class::reserved_marker, 0 }));
}

TEST(SimhUtilTest, GetPadding)
{
    EXPECT_EQ(0, GetPadding(0));
    EXPECT_EQ(0, GetPadding(6));
    EXPECT_EQ(1, GetPadding(7));
}

TEST(SimhUtilTest, FromLittleEndian)
{
    const array<uint8_t, META_DATA_SIZE> &data = { 0x01, 0x02, 0x03, 0x74 };
    EXPECT_EQ(simh_class::private_marker, FromLittleEndian(data).cls);
    EXPECT_EQ(0x04030201, FromLittleEndian(data).value);
}

TEST(SimhUtilTest, ToLittleEndian)
{
    const auto &data = ToLittleEndian(SimhMetaData { simh_class::private_marker, 0x01020304 });
    EXPECT_EQ(0x04, data[0]);
    EXPECT_EQ(0x03, data[1]);
    EXPECT_EQ(0x02, data[2]);
    EXPECT_EQ(0x71, data[3]);
}
