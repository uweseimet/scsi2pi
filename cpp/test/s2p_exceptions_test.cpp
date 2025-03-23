//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/s2p_exceptions.h"

TEST(S2pExceptionsTest, ParserException)
{
    try {
        throw ParserException("msg");
    } catch (const ParserException &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(S2pExceptionsTest, IoException)
{
    try {
        throw IoException("msg");
    } catch (const IoException &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(S2pExceptionsTest, ScsiException)
{
    try {
        throw ScsiException(SenseKey::UNIT_ATTENTION);
    } catch (const ScsiException &e) {
        EXPECT_EQ(SenseKey::UNIT_ATTENTION, e.GetSenseKey());
        EXPECT_EQ(Asc::NO_ADDITIONAL_SENSE_INFORMATION, e.GetAsc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }

    try {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::LBA_OUT_OF_RANGE);
    } catch (const ScsiException &e) {
        EXPECT_EQ(SenseKey::ILLEGAL_REQUEST, e.GetSenseKey());
        EXPECT_EQ(Asc::LBA_OUT_OF_RANGE, e.GetAsc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }
}
