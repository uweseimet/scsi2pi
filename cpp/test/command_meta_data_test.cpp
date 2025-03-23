//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/command_meta_data.h"

TEST(CommandMetaDataTest, LogCdb)
{
    const array<const uint8_t, 6> &cdb = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    EXPECT_EQ("? is executing REZERO/REWIND, CDB 01:02:03:04:05:06", CommandMetaData::GetInstance().LogCdb(cdb, "?"));
}
