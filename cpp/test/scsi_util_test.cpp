//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/scsi_util.h"

using namespace scsi_util;

TEST(ScsiUtilTest, GetInquiryProductData)
{
    vector<uint8_t> data(36);
    memcpy(data.data() + 8, "12345678", 8);
    memcpy(data.data() + 16, "1234567890123456", 16);
    memcpy(data.data() + 32, "1234", 4);
    const auto& [vendor, product, revision] = GetInquiryProductData(data);
    EXPECT_EQ("12345678", vendor);
    EXPECT_EQ("1234567890123456", product);
    EXPECT_EQ("1234", revision);
}

TEST(ScsiUtilTest, GetScsiLevel)
{
    EXPECT_EQ("-", GetScsiLevel(0));
    EXPECT_EQ("SCSI-1-CCS", GetScsiLevel(1));
    EXPECT_EQ("SCSI-2", GetScsiLevel(2));
    EXPECT_EQ("SCSI-3 (SPC)", GetScsiLevel(3));
    EXPECT_EQ("SPC-2", GetScsiLevel(4));
    EXPECT_EQ("SPC-3", GetScsiLevel(5));
    EXPECT_EQ("SPC-4", GetScsiLevel(6));
    EXPECT_EQ("SPC-5", GetScsiLevel(7));
    EXPECT_EQ("SPC-6", GetScsiLevel(8));
}

TEST(ScsiUtilTest, GetStatusString)
{
    EXPECT_NE(string::npos, GetStatusString(0x00).find("GOOD"));
    EXPECT_NE(string::npos, GetStatusString(0x02).find("CHECK CONDITION"));
    EXPECT_NE(string::npos, GetStatusString(0x04).find("CONDITION MET"));
    EXPECT_NE(string::npos, GetStatusString(0x08).find("BUSY"));
    EXPECT_NE(string::npos, GetStatusString(0x10).find("INTERMEDIATE"));
    EXPECT_NE(string::npos, GetStatusString(0x14).find("INTERMEDIATE-CONDITION MET"));
    EXPECT_NE(string::npos, GetStatusString(0x18).find("RESERVATION CONFLICT"));
    EXPECT_NE(string::npos, GetStatusString(0x22).find("COMMAND TERMINATED"));
    EXPECT_NE(string::npos, GetStatusString(0x28).find("QUEUE FULL"));
    EXPECT_NE(string::npos, GetStatusString(0x30).find("ACA ACTIVE"));
    EXPECT_NE(string::npos, GetStatusString(0x40).find("TASK ABORTED"));
    EXPECT_NE(string::npos, GetStatusString(0xfe).find("unknown"));
    EXPECT_NE(string::npos, GetStatusString(0xff).find("respond"));
}

TEST(ScsiUtilTest, FormatSenseData)
{
    EXPECT_EQ("ABORTED COMMAND (Sense Key $0b), COMMAND PHASE ERROR (ASC $4a), ASCQ $00", FormatSenseData(
        ABORTED_COMMAND, COMMAND_PHASE_ERROR, NO_QUALIFIER));
    EXPECT_EQ("ABORTED COMMAND (Sense Key $0b), ASC $ff, ASCQ $ff", FormatSenseData(
        ABORTED_COMMAND, static_cast<Asc>(0xff), static_cast<Ascq>(0xff)));
}
