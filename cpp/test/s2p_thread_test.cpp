//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2p/s2p_thread.h"

TEST(S2pThreadTest, Init)
{
    S2pThread service_thread;

    EXPECT_TRUE(service_thread.Init(9999, nullptr,default_logger()).empty())
        << "Port 9999 is expected not to be in use for this test";
    service_thread.Stop();
}

#if __has_include(<sys/socket.h>)
TEST(S2pThreadTest, IsRunning)
{
    S2pThread service_thread;
    EXPECT_FALSE(service_thread.IsRunning());
    EXPECT_TRUE(service_thread.Init(9999, nullptr, default_logger()).empty())
        << "Port 9999 is expected not to be in use for this test";
    EXPECT_FALSE(service_thread.IsRunning());

    service_thread.Start();
    EXPECT_TRUE(service_thread.IsRunning());
    service_thread.Stop();
    EXPECT_FALSE(service_thread.IsRunning());
}
#endif
