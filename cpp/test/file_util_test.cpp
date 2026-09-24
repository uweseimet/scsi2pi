//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <filesystem>
#include "test_shared.h"
#include "shared/file_util.h"
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace file_util;
using namespace s2p_test;

TEST(FileUtilTest, IsReadyOnlyFile)
{
    EXPECT_TRUE("/tmp/xyz");
}

TEST(FileUtilTest, GetExtensionLowerCase)
{
    EXPECT_EQ("ext", GetExtensionLowerCase(path("test.ext")));
    EXPECT_EQ("ext", GetExtensionLowerCase(path("test.EXT")));
    EXPECT_EQ("ext", GetExtensionLowerCase(path("test.1.EXT")));
}

TEST(FileUtilTest, GetCapacityFromFile)
{
    const path &filename = CreateTempFile(512);
    EXPECT_EQ(512, GetCapacityFromFile(filename));

    EXPECT_THROW(GetCapacityFromFile("/dev/null"), IoException);

    EXPECT_THROW(GetCapacityFromFile("/non_existing_file"), IoException);
}
