//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <filesystem>
#include "shared/user_util.h"

using namespace filesystem;
using namespace user_util;

TEST(UserUtilTest, GetAppDir)
{
    if (!GetEuid()) {
        error_code error;
        EXPECT_EQ(exists("/home/pi", error) ? "/home/pi" : "/var/lib/piscsi", GetAppDir());
    }
}

TEST(UserUtilTest, GetUidAndGid)
{
    EXPECT_EQ(GetEuid(), GetUidAndGid().first);
}
