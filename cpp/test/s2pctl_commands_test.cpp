//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "test/test_shared.h"
#include "shared/shared_exceptions.h"
#include "protobuf/protobuf_util.h"
#include "s2pctl/s2pctl_commands.h"

using namespace testing;
using namespace protobuf_util;

TEST(S2pCtlCommandsTest, Execute)
{
    PbCommand command;
    S2pCtlCommands commands(command, "localhost", 0, "", "", "");

    command.set_operation(LOG_LEVEL);
    EXPECT_THROW(commands.Execute("log_level", "", "", "", ""), io_exception);
    EXPECT_EQ("log_level", GetParam(command, "level"));

    command.set_operation(DEFAULT_FOLDER);
    EXPECT_THROW(commands.Execute("", "default_folder", "", "", ""), io_exception);
    EXPECT_EQ("default_folder", GetParam(command, "folder"));

    command.set_operation(RESERVE_IDS);
    EXPECT_THROW(commands.Execute("", "", "reserved_ids", "", ""), io_exception);
    EXPECT_EQ("reserved_ids", GetParam(command, "ids"));

    command.set_operation(CREATE_IMAGE);
    EXPECT_FALSE(commands.Execute("", "", "", "", ""));
    EXPECT_THROW(commands.Execute("", "", "", "filename:0", ""), io_exception);
    EXPECT_EQ("false", GetParam(command, "read_only"));

    command.set_operation(DELETE_IMAGE);
    EXPECT_THROW(commands.Execute("", "", "", "filename1", ""), io_exception);
    EXPECT_EQ("filename1", GetParam(command, "file"));

    command.set_operation(RENAME_IMAGE);
    EXPECT_FALSE(commands.Execute("", "", "", "", ""));
    EXPECT_THROW(commands.Execute("", "", "", "from1:to1", ""), io_exception);
    EXPECT_EQ("from1", GetParam(command, "from"));
    EXPECT_EQ("to1", GetParam(command, "to"));

    command.set_operation(COPY_IMAGE);
    EXPECT_FALSE(commands.Execute("", "", "", "", ""));
    EXPECT_THROW(commands.Execute("", "", "", "from2:to2", ""), io_exception);
    EXPECT_EQ("from2", GetParam(command, "from"));
    EXPECT_EQ("to2", GetParam(command, "to"));

    command.set_operation(DEVICES_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(DEVICE_TYPES_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(VERSION_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(SERVER_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(DEFAULT_IMAGE_FILES_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(IMAGE_FILE_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", "filename2"), io_exception);
    EXPECT_EQ("filename2", GetParam(command, "file"));

    command.set_operation(NETWORK_INTERFACES_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(LOG_LEVEL_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(RESERVED_IDS_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(MAPPING_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(STATISTICS_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(PROPERTIES_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(OPERATION_INFO);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(DETACH_ALL);
    EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

    command.set_operation(NO_OPERATION);
    EXPECT_FALSE(commands.Execute("", "", "", "", ""));
}

TEST(S2pCtlCommandsTest, CommandDevicesInfo)
{
    PbCommand command;

    S2pCtlCommands commands1(command, "/invalid_host_name", 0, "", "", "");
    EXPECT_THROW(commands1.CommandDevicesInfo(), io_exception);

    S2pCtlCommands commands2(command, "localhost", 0, "", "", "");
    EXPECT_THROW(commands2.CommandDevicesInfo(), io_exception);
}

TEST(S2pCtlCommandsTest, Export)
{
    PbCommand command;
    command.set_operation(OPERATION_INFO);

    auto [fd_bin, filename_bin] = OpenTempFile();
    S2pCtlCommands commands1(command, "localhost", 0, filename_bin, "", "");
    EXPECT_TRUE(commands1.Execute("", "", "", "", ""));
    EXPECT_EQ(2U, file_size(filename_bin));

    auto [fd_json, filename_json] = OpenTempFile();
    S2pCtlCommands commands2(command, "localhost", 0, "", filename_json, "");
    EXPECT_TRUE(commands2.Execute("", "", "", "", ""));
    EXPECT_NE(string::npos, ReadTempFileToString(filename_json).find(PbOperation_Name(OPERATION_INFO)));

    auto [fd_txt, filename_txt] = OpenTempFile();
    S2pCtlCommands commands3(command, "localhost", 0, "", "", filename_txt);
    EXPECT_TRUE(commands3.Execute("", "", "", "", ""));
    EXPECT_NE(string::npos, ReadTempFileToString(filename_txt).find(PbOperation_Name(OPERATION_INFO)));
}

