//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include "shared/network_util.h"

using namespace network_util;

#ifdef __linux__
TEST(NetworkUtilTest, GetNetworkInterfaces)
{
    EXPECT_FALSE(GetNetworkInterfaces().empty());
}
#endif

TEST(NetworkUtilTest, ResolveHostName)
{
    sockaddr_in server_addr = { };
    EXPECT_FALSE(ResolveHostName("foo.foobar", &server_addr));
    EXPECT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
}
