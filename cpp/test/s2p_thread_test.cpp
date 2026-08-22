//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#endif
#if __has_include(<arpa/inet.h>)
#include <arpa/inet.h>
#endif
#include <unistd.h>
#include <gtest/gtest.h>
#include "command/command_context.h"
#include "protobuf/protobuf_util.h"
#include "s2p/s2p_thread.h"
#include "shared/network_util.h"
#include "shared/s2p_exceptions.h"

using namespace protobuf_util;
using namespace network_util;

#if __has_include(<sys/socket.h>)
void SendCommand(const PbCommand &command, PbResult &result)
{
    auto server_addr = ResolveHostName("127.0.0.1");
    ASSERT_TRUE(server_addr.has_value());
    server_addr->sin_port = htons(uint16_t(9999));

    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(-1, fd);
    EXPECT_TRUE(connect(fd, reinterpret_cast<sockaddr*>(&(*server_addr)), sizeof(*server_addr)) >= 0)
        << "Service should be running";
    ASSERT_EQ(6, write(fd, "RASCSI", 6));
    SerializeMessage(fd, command);
    DeserializeMessage(fd, result);
    close(fd);
}
#endif

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

#if __has_include(<sys/socket.h>)
TEST(S2pThreadTest, Execute)
{
    S2pThread service_thread;
    service_thread.Init(9999, [](const CommandContext &context) {
        PbResult result;
        result.set_status(context.GetCommand().operation() == PbOperation::NO_OPERATION);
        return context.WriteResult(result);
    }, default_logger());

    service_thread.Start();

    PbCommand command;
    PbResult result;

    command.set_operation(PbOperation::NO_OPERATION);
    SendCommand(command, result);
    EXPECT_TRUE(result.status()) << "Command should have succeeded";

    command.set_operation(PbOperation::EJECT);
    SendCommand(command, result);
    EXPECT_FALSE(result.status()) << "Command should have failed";

    service_thread.Stop();
}
#endif
