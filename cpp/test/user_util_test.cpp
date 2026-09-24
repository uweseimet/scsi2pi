//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include "shared/user_util.h"

using namespace filesystem;
using namespace user_util;

TEST(UserUtilTest, GetAppDir)
{
    unsetenv("SUDO_UID");

    if (!GetEuid()) {
        error_code error;
        EXPECT_EQ(exists("/home/pi", error) ? "/home/pi" : "/var/lib/piscsi", GetAppDir());
    }

    setenv("SUDO_UID", "-1", 0);
    EXPECT_EQ(DEFAULT_APP_FOLDER, GetAppDir());

    unsetenv("SUDO_UID");
}

TEST(UserUtilTest, GetUidAndGid)
{
    unsetenv("SUDO_UID");

    EXPECT_EQ(GetEuid(), GetUidAndGid().first);

    setenv("SUDO_UID", "-1", 0);
    const auto [uid, gid] = GetUidAndGid();
    EXPECT_EQ(-1, uid);
    EXPECT_EQ(-1, gid);

    unsetenv("SUDO_UID");
}
