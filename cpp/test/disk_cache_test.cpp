//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "test_shared.h"
#include "devices/disk_cache.h"

using namespace testing;

TEST(DiskCache, Constructor)
{
    DiskCache cache1("", 512, 0, false);
    EXPECT_FALSE(cache1.IsRawMode());

    DiskCache cache2("", 512, 0, true);
    EXPECT_TRUE(cache2.IsRawMode());
}

TEST(DiskCache, Init)
{
    DiskCache cache1("", 512, 0, false);
    EXPECT_FALSE(cache1.Init());

    DiskCache cache5("test", 512, 1, false);
    EXPECT_TRUE(cache5.Init());
}

TEST(DiskCache, ReadWriteSectors)
{
    vector<uint8_t> buf(512);
    DiskCache cache(CreateTempFile(buf.size()), static_cast<int>(buf.size()), 1, false);
    EXPECT_TRUE(cache.Init());

    EXPECT_EQ(0, cache.ReadSectors(buf, 1, 1));
    EXPECT_EQ(0, cache.WriteSectors(buf, 1, 1));

    buf[1] = 123;
    EXPECT_EQ(512, cache.WriteSectors(buf, 0, 1));
    buf[1] = 0;

    EXPECT_EQ(512, cache.ReadSectors(buf, 0, 1));
    EXPECT_EQ(123, buf[1]);
}

TEST(DiskCache, GetStatistics)
{
    DiskCache cache("", 512, 0, false);

    EXPECT_EQ(2U, cache.GetStatistics(true).size());
    EXPECT_EQ(4U, cache.GetStatistics(false).size());
}
