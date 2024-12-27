//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/scsi.h"
#include "shared/sg_util.h"

using namespace memory_util;
using namespace sg_util;

TEST(SgUtilTest, OpenDevice)
{
    EXPECT_THROW(OpenDevice("/dev/null"), IoException);
    EXPECT_THROW(OpenDevice("/dev/sg12345"), IoException);
}

TEST(SgUtilTest, GetAllocationLength)
{
    vector<uint8_t> cdb(16);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CAPACITY_10);
    EXPECT_EQ(8, GetAllocationLength(cdb));

    cdb[0] = static_cast<uint8_t>(ScsiCommand::FORMAT_UNIT);
    EXPECT_EQ(0, GetAllocationLength(cdb));

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_6);
    EXPECT_EQ(0, GetAllocationLength(cdb));

    cdb[0] = static_cast<uint8_t>(ScsiCommand::INQUIRY);
    cdb[4] = 10;
    EXPECT_EQ(10, GetAllocationLength(cdb));
    cdb[4] = 0;

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_LONG_10);
    SetInt16(cdb, 7, 0x1234);
    EXPECT_EQ(0x1234, GetAllocationLength(cdb));
    SetInt16(cdb, 7, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CD);
    SetInt24(cdb, 6, 0x123456);
    EXPECT_EQ(0x123456, GetAllocationLength(cdb));
    SetInt24(cdb, 6, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::REPORT_LUNS);
    SetInt32(cdb, 6, 0x12345678);
    EXPECT_EQ(0x12345678, GetAllocationLength(cdb));
}

TEST(SgUtilTest, UpdateStartBlock)
{
    vector<uint8_t> cdb(6);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::FORMAT_UNIT);
    UpdateStartBlock(cdb, 255);
    EXPECT_EQ(0U, GetInt24(cdb, 1));

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_6);
    UpdateStartBlock(cdb, 0x123456);
    EXPECT_EQ(0x123456U, GetInt24(cdb, 1));
    SetInt24(cdb, 1, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_6);
    UpdateStartBlock(cdb, 0x654321);
    EXPECT_EQ(0x654321U, GetInt24(cdb, 1));
    SetInt24(cdb, 1, 0);

    cdb.resize(10);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_10);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_10);
    UpdateStartBlock(cdb, 0x87654321);
    EXPECT_EQ(0x87654321U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::VERIFY_10);
    UpdateStartBlock(cdb, 0x87654321);
    EXPECT_EQ(0x87654321U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb.resize(16);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
    SetInt64(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
    SetInt64(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::VERIFY_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
}

TEST(SgUtilTest, SetBlockCount)
{
    vector<uint8_t> cdb(6);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::FORMAT_UNIT);
    SetBlockCount(cdb, 255);
    EXPECT_EQ(0, cdb[4]);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_6);
    SetBlockCount(cdb, 1);
    EXPECT_EQ(1, cdb[4]);
    cdb[4] = 0;

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_6);
    SetBlockCount(cdb, 2);
    EXPECT_EQ(2, cdb[4]);
    cdb[4] = 0;

    cdb.resize(10);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_10);
    SetBlockCount(cdb, 12345);
    EXPECT_EQ(12345U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_10);
    SetBlockCount(cdb, 54321);
    EXPECT_EQ(54321U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::VERIFY_10);
    SetBlockCount(cdb, 12345);
    EXPECT_EQ(12345U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb.resize(16);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_16);
    SetBlockCount(cdb, 12345678);
    EXPECT_EQ(12345678U, GetInt32(cdb, 10));
    SetInt16(cdb, 10, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_16);
    SetBlockCount(cdb, 87654321);
    EXPECT_EQ(87654321U, GetInt32(cdb, 10));
    SetInt16(cdb, 10, 0);

    cdb[0] = static_cast<uint8_t>(ScsiCommand::VERIFY_16);
    SetBlockCount(cdb, 12345678);
    EXPECT_EQ(12345678U, GetInt32(cdb, 10));
    SetInt16(cdb, 10, 0);
}

TEST(SgUtilTest, SetInt24)
{
    vector<uint8_t> buf(4);

    SetInt24(buf, 1, 0x123456);
    EXPECT_EQ(0x123456U, GetInt24(buf, 1));
}
