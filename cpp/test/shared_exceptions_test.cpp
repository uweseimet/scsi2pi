//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/shared_exceptions.h"

using namespace scsi_defs;

TEST(SharedExceptionsTest, IoException)
{
    try {
        throw io_exception("msg");
        FAIL();
    } catch (const io_exception &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(SharedExceptionsTest, FileNotFoundException)
{
    try {
        throw file_not_found_exception("msg");
        FAIL();
    } catch (const file_not_found_exception &e) {
        EXPECT_STREQ("msg", e.what());
    }
}

TEST(SharedExceptionsTest, ScsiException)
{
    try {
        throw scsi_exception(sense_key::unit_attention);
        FAIL();
    } catch (const scsi_exception &e) {
        EXPECT_EQ(sense_key::unit_attention, e.get_sense_key());
        EXPECT_EQ(asc::no_additional_sense_information, e.get_asc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }

    try {
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
        FAIL();
    } catch (const scsi_exception &e) {
        EXPECT_EQ(sense_key::illegal_request, e.get_sense_key());
        EXPECT_EQ(asc::lba_out_of_range, e.get_asc());
        EXPECT_NE(nullptr, strstr(e.what(), "Sense Key"));
        EXPECT_NE(nullptr, strstr(e.what(), "ASC"));
    }
}
