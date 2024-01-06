//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include "shared/network_util.h"

using namespace network_util;

TEST(NetworkUtilTest, IsInterfaceUp)
{
    EXPECT_FALSE(IsInterfaceUp("foo_bar"));
}

TEST(NetworkUtilTest, ResolveHostName)
{
    sockaddr_in server_addr = { };
    EXPECT_FALSE(ResolveHostName("foo.foobar", &server_addr));
    EXPECT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
}
