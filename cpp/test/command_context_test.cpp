//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fcntl.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include "command/command_context.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"
#include "test_shared.h"

using namespace testing;
using namespace protobuf_util;

TEST(CommandContext, ReadCommand)
{
    int fd = open(CreateTempFile(0).string().c_str(), O_RDONLY);
    CommandContext context1(fd, *default_logger());
    EXPECT_FALSE(context1.ReadCommand());
    close(fd);

    // Invalid magic with wrong length
    vector data = { byte { '1' }, byte { '2' }, byte { '3' } };
    fd = open(CreateTempFileWithData(data).c_str(), O_RDONLY);
    CommandContext context2(fd, *default_logger());
    EXPECT_THROW(context2.ReadCommand(), IoException);
    close(fd);

    // Invalid magic with right length
    data = { byte { '1' }, byte { '2' }, byte { '3' }, byte { '4' }, byte { '5' }, byte { '6' } };
    fd = open(CreateTempFileWithData(data).c_str(), O_RDONLY);
    CommandContext context3(fd, *default_logger());
    EXPECT_THROW(context3.ReadCommand(), IoException);
    close(fd);

    data = { byte { 'R' }, byte { 'A' }, byte { 'S' }, byte { 'C' }, byte { 'S' }, byte { 'I' }, byte { '1' } };
    // Valid magic but invalid command
    fd = open(CreateTempFileWithData(data).c_str(), O_RDONLY);
    CommandContext context4(fd, *default_logger());
    EXPECT_THROW(context4.ReadCommand(), IoException);
    close(fd);

    data = { byte { 'R' }, byte { 'A' }, byte { 'S' }, byte { 'C' }, byte { 'S' }, byte { 'I' } };
    // Valid magic but missing command
    fd = open(CreateTempFileWithData(data).c_str(), O_RDONLY);
    CommandContext context5(fd, *default_logger());
    EXPECT_THROW(context5.ReadCommand(), IoException);
    close(fd);

    const string &filename = CreateTempFileWithData(data);
    fd = open(filename.c_str(), O_RDWR | O_APPEND);
    PbCommand command;
    command.set_operation(PbOperation::SERVER_INFO);
    SerializeMessage(fd, command);
    lseek(fd, 0, SEEK_SET);
    CommandContext context6(fd, *default_logger());
    EXPECT_TRUE(context6.ReadCommand());
    close(fd);
    EXPECT_EQ(PbOperation::SERVER_INFO, context6.GetCommand().operation());
}

TEST(CommandContext, GetCommand)
{
    PbCommand command;
    command.set_operation(PbOperation::SERVER_INFO);
    CommandContext context(command, *default_logger());
    EXPECT_EQ(PbOperation::SERVER_INFO, context.GetCommand().operation());
}

TEST(CommandContext, GetLogger)
{
    PbCommand command;
    CommandContext context(command, *default_logger());
    EXPECT_EQ(default_logger()->name(), context.GetLogger().name());
}

TEST(CommandContext, WriteResult)
{
    const string &filename = CreateTempFile(0);
    const int fd = open(filename.c_str(), O_RDWR | O_APPEND);
    PbResult result;
    result.set_error_code(PbErrorCode::UNAUTHORIZED);
    CommandContext context(fd, *default_logger());
    EXPECT_FALSE(context.WriteResult(result));
    EXPECT_FALSE(result.status());
    lseek(fd, 0, SEEK_SET);
    result.set_status(true);
    DeserializeMessage(fd, result);
    close(fd);
    EXPECT_FALSE(result.status());
    EXPECT_EQ(PbErrorCode::UNAUTHORIZED, result.error_code());
}

TEST(CommandContext, WriteSuccessResult)
{
    PbResult result;
    PbCommand command;
    CommandContext context(command, *default_logger());
    EXPECT_TRUE(context.WriteSuccessResult(result));
    EXPECT_TRUE(result.status());
}

TEST(CommandContext, ReturnLocalizedError)
{
    PbCommand command;
    CommandContext context(command, *default_logger());
    context.SetLocale("en_US");

    EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}

TEST(CommandContext, ReturnSuccessStatus)
{
    PbCommand command;

    CommandContext context1(command, *default_logger());
    EXPECT_TRUE(context1.ReturnSuccessStatus());

    const int fd = open("/dev/null", O_RDWR);
    CommandContext context2(fd, *default_logger());
    EXPECT_TRUE(context2.ReturnSuccessStatus());
    close(fd);
}

TEST(CommandContext, ReturnErrorStatus)
{
    PbCommand command;

    CommandContext context1(command, *default_logger());
    EXPECT_FALSE(context1.ReturnErrorStatus("error"));

    const int fd = open("/dev/null", O_RDWR);
    CommandContext context2(fd, *default_logger());
    EXPECT_FALSE(context2.ReturnErrorStatus("error"));
    close(fd);
}
