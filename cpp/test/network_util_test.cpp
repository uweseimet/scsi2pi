//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/network_util.h"

using namespace network_util;

TEST(NetworkUtilTest, GetNetworkInterfaces)
{
    EXPECT_FALSE(GetNetworkInterfaces().empty());
}

#if __has_include(<netinet/in.h>)
TEST(NetworkUtilTest, ResolveHostName)
{
    EXPECT_FALSE(ResolveHostName("foo.foobar").has_value());
    EXPECT_TRUE(ResolveHostName("127.0.0.1").has_value());
}
#endif
