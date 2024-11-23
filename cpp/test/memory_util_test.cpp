//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/memory_util.h"

using namespace memory_util;

TEST(MemoryUtilTest, GetInt16)
{
    const vector<uint8_t> b = { 0xfe, 0xdc };
    EXPECT_EQ(0xfedc, GetInt16(b, 0));

    const vector<int> v = { 0x12, 0x34 };
    EXPECT_EQ(0x1234, GetInt16(v, 0));
}

TEST(MemoryUtilTest, GetInt24)
{
    vector<int> v = { 0x12, 0x34, 0x56 };
    EXPECT_EQ(0x123456, GetInt24(v, 0));

    v = { 0xf2, 0x34, 0x56 };
    EXPECT_EQ(0xf23456, GetInt24(v, 0));
}

TEST(MemoryUtilTest, GetSignedInt24)
{
    vector<int> v = { 0x00, 0x00, 0x00 };
    EXPECT_EQ(0, GetSignedInt24(v, 0));

    v = { 0x00, 0x00, 0x01 };
    EXPECT_EQ(1, GetSignedInt24(v, 0));

    v = { 0xff, 0xff, 0xff };
    EXPECT_EQ(-1, GetSignedInt24(v, 0));

    v = { 0xff, 0xff, 0xfe };
    EXPECT_EQ(-2, GetSignedInt24(v, 0));
}

TEST(MemoryUtilTest, GetInt32)
{
    const vector<int> v = { 0x12, 0x34, 0x56, 0x78 };
    EXPECT_EQ(0x12345678U, GetInt32(v, 0));
}

TEST(MemoryUtilTest, GetInt64)
{
    const vector<int> v = { 0x12, 0x34, 0x56, 0x78, 0x87, 0x65, 0x43, 0x21 };
    EXPECT_EQ(0x1234567887654321U, GetInt64(v, 0));
}

TEST(MemoryUtilTest, SetInt16)
{
    vector<byte> v(2);
    SetInt16(v, 0, 0x1234);
    EXPECT_EQ(byte { 0x12 }, v[0]);
    EXPECT_EQ(byte { 0x34 }, v[1]);
}

TEST(MemoryUtilTest, SetInt32)
{
    vector<uint8_t> buf(4);
    SetInt32(buf, 0, 0x12345678);
    EXPECT_EQ(0x12, buf[0]);
    EXPECT_EQ(0x34, buf[1]);
    EXPECT_EQ(0x56, buf[2]);
    EXPECT_EQ(0x78, buf[3]);

    vector<byte> v(4);
    SetInt32(v, 0, 0x12345678);
    EXPECT_EQ(byte { 0x12 }, v[0]);
    EXPECT_EQ(byte { 0x34 }, v[1]);
    EXPECT_EQ(byte { 0x56 }, v[2]);
    EXPECT_EQ(byte { 0x78 }, v[3]);
}

TEST(MemoryUtilTest, SetInt64)
{
    array<uint8_t, 8> buf;
    SetInt64(buf, 0, 0x1234567887654321);
    EXPECT_EQ(0x12, buf[0]);
    EXPECT_EQ(0x34, buf[1]);
    EXPECT_EQ(0x56, buf[2]);
    EXPECT_EQ(0x78, buf[3]);
    EXPECT_EQ(0x87, buf[4]);
    EXPECT_EQ(0x65, buf[5]);
    EXPECT_EQ(0x43, buf[6]);
    EXPECT_EQ(0x21, buf[7]);
}
