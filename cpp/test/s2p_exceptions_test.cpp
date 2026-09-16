//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/s2p_exceptions.h"

using enum SenseKey;
using enum Asc;

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
        throw ScsiException(UNIT_ATTENTION);
    } catch (const ScsiException &e) {
        EXPECT_EQ(UNIT_ATTENTION, e.GetSenseKey());
        EXPECT_EQ(NO_ADDITIONAL_SENSE_INFORMATION, e.GetAsc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }

    try {
        throw ScsiException(ILLEGAL_REQUEST, LBA_OUT_OF_RANGE);
    } catch (const ScsiException &e) {
        EXPECT_EQ(ILLEGAL_REQUEST, e.GetSenseKey());
        EXPECT_EQ(LBA_OUT_OF_RANGE, e.GetAsc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }
}
