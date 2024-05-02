//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include "command/command_context.h"
#include "protobuf/protobuf_util.h"
#include "s2p/s2p_thread.h"
#include "shared/network_util.h"
#include "shared/s2p_exceptions.h"

using namespace protobuf_util;
using namespace network_util;

void SendCommand(const PbCommand &command, PbResult &result)
{
    sockaddr_in server_addr = { };
    ASSERT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
    server_addr.sin_port = htons(uint16_t(9999));

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(-1, fd);
    EXPECT_TRUE(connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) >= 0)
    << "Service should be running"; // NOSONAR bit_cast is not supported by the bullseye clang++ compiler
    ASSERT_EQ(6, write(fd, "RASCSI", 6));
    SerializeMessage(fd, command);
    DeserializeMessage(fd, result);
    close(fd);
}

TEST(S2pThreadTest, Init)
{
    S2pThread service_thread;

    EXPECT_FALSE(service_thread.Init(nullptr, 65536).empty()) << "Illegal port number";
    EXPECT_FALSE(service_thread.Init(nullptr, 0).empty()) << "Illegal port number";
    EXPECT_FALSE(service_thread.Init(nullptr, -1).empty()) << "Illegal port number";
    EXPECT_FALSE(service_thread.Init(nullptr, 1).empty()) << "Port 1 is only available for the root user";
    EXPECT_TRUE(service_thread.Init(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
    service_thread.Stop();
}

TEST(S2pThreadTest, IsRunning)
{
    S2pThread service_thread;
    EXPECT_FALSE(service_thread.IsRunning());
    EXPECT_TRUE(service_thread.Init(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
    EXPECT_FALSE(service_thread.IsRunning());

    service_thread.Start();
    EXPECT_TRUE(service_thread.IsRunning());
    service_thread.Stop();
    EXPECT_FALSE(service_thread.IsRunning());
}

TEST(S2pThreadTest, Execute)
{
    sockaddr_in server_addr = { };
    ASSERT_TRUE(ResolveHostName("127.0.0.1", &server_addr));

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(-1, fd);

    server_addr.sin_port = htons(uint16_t(9999));
    EXPECT_FALSE(connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) >= 0)
    << "Service should not be running"; // NOSONAR bit_cast is not supported by the bullseye clang++ compiler

    close(fd);

    S2pThread service_thread;
    service_thread.Init([](const CommandContext &context) {
        if (context.GetCommand().operation() != PbOperation::NO_OPERATION) {
            throw io_exception("error");
        }

        PbResult result;
        result.set_status(true);
        return context.WriteResult(result);
    }, 9999);

    service_thread.Start();

    PbCommand command;
    PbResult result;

    SendCommand(command, result);
    command.set_operation(PbOperation::NO_OPERATION);
    EXPECT_TRUE(result.status()) << "Command should have been successful";

    command.set_operation(PbOperation::EJECT);
    SendCommand(command, result);
    EXPECT_FALSE(result.status()) << "Exception should have been raised";

    service_thread.Stop();
}
