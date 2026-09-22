//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "command/command_context.h"
#include "command/command_image_support.h"
#include "protobuf/s2p_interface_util.h"

using namespace command_image_support;
using namespace s2p_interface_util;

TEST(CommandImageSupportTest, Depth)
{
    SetDepth(1);
    EXPECT_EQ(1, GetDepth());
}

TEST(CommandImageSupportTest, DefaultFolder)
{
    EXPECT_NE(string::npos, GetImageFolder().find("/images"));

    EXPECT_FALSE(SetImageFolder("").empty());
    EXPECT_FALSE(SetImageFolder("/not_in_home").empty());

    error_code error;
    if (exists("/var/lib/piscsi/images", error)) {
        EXPECT_TRUE(SetImageFolder("/var/lib/piscsi/images").empty());
    }

    EXPECT_TRUE(SetImageFolder(temp_directory_path().string()).empty());
}

TEST(CommandImageSupportTest, CreateImage)
{
    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1, *default_logger());
    EXPECT_FALSE(CreateImage(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2, *default_logger());
    EXPECT_FALSE(CreateImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "file", "filename");
    SetParam(command3, "size", "-1");
    CommandContext context3(command3, *default_logger());
    EXPECT_FALSE(CreateImage(context3)) << "Size must be reported as invalid";

    PbCommand command4;
    SetParam(command4, "file", "filename");
    SetParam(command4, "size", "");
    CommandContext context4(command4, *default_logger());
    EXPECT_FALSE(CreateImage(context4)) << "Size must be reported as missing";

    PbCommand command5;
    SetParam(command5, "size", "1");
    CommandContext context5(command5, *default_logger());
    EXPECT_FALSE(CreateImage(context5)) << "Size must be reported as invalid";

    PbCommand command6;
    SetParam(command6, "size", "513");
    CommandContext context6(command6, *default_logger());
    EXPECT_FALSE(CreateImage(context6)) << "Size must be reported as not a multiple of 512";

    // Further tests would modify the filesystem
}

TEST(CommandImageSupportTest, DeleteImage)
{
    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1, *default_logger());
    EXPECT_FALSE(DeleteImage(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2, *default_logger());
    EXPECT_FALSE(DeleteImage(context2)) << "Depth must be reported as invalid";

    MockStorageDevice device;
    device.SetFilename("filename");
    device.ReserveFile();
    PbCommand command3;
    SetParam(command3, "file", "filename");
    CommandContext context3(command3, *default_logger());
    EXPECT_FALSE(DeleteImage(context3)) << "File must be reported as in use";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, RenameImage)
{
    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1, *default_logger());
    EXPECT_FALSE(RenameImage(context1)) << "Source filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "from", "/a/b/c/filename_from");
    CommandContext context2(command2, *default_logger());
    EXPECT_FALSE(RenameImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "from", "filename_from");
    CommandContext context3(command3, *default_logger());
    EXPECT_FALSE(RenameImage(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, CopyImage)
{
    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1, *default_logger());
    EXPECT_FALSE(CopyImage(context1)) << "Source filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "from", "/a/b/c/filename_from");
    CommandContext context2(command2, *default_logger());
    EXPECT_FALSE(CopyImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "from", "filename_from");
    CommandContext context3(command3, *default_logger());
    EXPECT_FALSE(CopyImage(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, SetImagePermissions)
{
    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1, *default_logger());
    EXPECT_FALSE(SetImagePermissions(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2, *default_logger());
    EXPECT_FALSE(SetImagePermissions(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "file", "filename");
    CommandContext context3(command3, *default_logger());
    EXPECT_FALSE(SetImagePermissions(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}
