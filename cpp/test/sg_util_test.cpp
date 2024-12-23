//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/memory_util.h"
#include "shared/scsi.h"
#include "shared/sg_util.h"

using namespace memory_util;
using namespace sg_util;

TEST(SgUtilTest, UpdateStartBlock)
{
    vector<uint8_t> cdb(6);

    cdb[0] = static_cast<uint8_t>(scsi_command::format_unit);
    UpdateStartBlock(cdb, 255);
    EXPECT_EQ(0U, GetInt24(cdb, 1));

    cdb[0] = static_cast<uint8_t>(scsi_command::read_6);
    UpdateStartBlock(cdb, 0x123456);
    EXPECT_EQ(0x123456U, GetInt24(cdb, 1));
    SetInt24(cdb, 1, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::write_6);
    UpdateStartBlock(cdb, 0x654321);
    EXPECT_EQ(0x654321U, GetInt24(cdb, 1));
    SetInt24(cdb, 1, 0);

    cdb.resize(10);

    cdb[0] = static_cast<uint8_t>(scsi_command::read_10);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::write_10);
    UpdateStartBlock(cdb, 0x87654321);
    EXPECT_EQ(0x87654321U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::verify_10);
    UpdateStartBlock(cdb, 0x87654321);
    EXPECT_EQ(0x87654321U, GetInt32(cdb, 2));
    SetInt32(cdb, 2, 0);

    cdb.resize(16);

    cdb[0] = static_cast<uint8_t>(scsi_command::read_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
    SetInt64(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::write_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
    SetInt64(cdb, 2, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::verify_16);
    UpdateStartBlock(cdb, 0x12345678);
    EXPECT_EQ(0x12345678U, GetInt64(cdb, 2));
}

TEST(SgUtilTest, SetBlockCount)
{
    vector<uint8_t> cdb(6);

    cdb[0] = static_cast<uint8_t>(scsi_command::format_unit);
    SetBlockCount(cdb, 255);
    EXPECT_EQ(0, cdb[4]);

    cdb[0] = static_cast<uint8_t>(scsi_command::read_6);
    SetBlockCount(cdb, 1);
    EXPECT_EQ(1, cdb[4]);
    cdb[4] = 0;

    cdb[0] = static_cast<uint8_t>(scsi_command::write_6);
    SetBlockCount(cdb, 2);
    EXPECT_EQ(2, cdb[4]);
    cdb[4] = 0;

    cdb.resize(10);

    cdb[0] = static_cast<uint8_t>(scsi_command::read_10);
    SetBlockCount(cdb, 12345);
    EXPECT_EQ(12345U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::write_10);
    SetBlockCount(cdb, 54321);
    EXPECT_EQ(54321U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::verify_10);
    SetBlockCount(cdb, 12345);
    EXPECT_EQ(12345U, GetInt16(cdb, 7));
    SetInt16(cdb, 7, 0);

    cdb.resize(16);

    cdb[0] = static_cast<uint8_t>(scsi_command::read_16);
    SetBlockCount(cdb, 12345678);
    EXPECT_EQ(12345678U, GetInt32(cdb, 10));
    SetInt16(cdb, 10, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::write_16);
    SetBlockCount(cdb, 87654321);
    EXPECT_EQ(87654321U, GetInt32(cdb, 10));
    SetInt16(cdb, 10, 0);

    cdb[0] = static_cast<uint8_t>(scsi_command::verify_16);
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
