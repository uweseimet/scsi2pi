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
        throw parser_exception("msg");
    } catch (const parser_exception &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(S2pExceptionsTest, IoException)
{
    try {
        throw io_exception("msg");
    } catch (const io_exception &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(S2pExceptionsTest, ScsiException)
{
    try {
        throw scsi_exception(sense_key::unit_attention);
    } catch (const scsi_exception &e) {
        EXPECT_EQ(sense_key::unit_attention, e.get_sense_key());
        EXPECT_EQ(asc::no_additional_sense_information, e.get_asc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }

    try {
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    } catch (const scsi_exception &e) {
        EXPECT_EQ(sense_key::illegal_request, e.get_sense_key());
        EXPECT_EQ(asc::lba_out_of_range, e.get_asc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }
}
