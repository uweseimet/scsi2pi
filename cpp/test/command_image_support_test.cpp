//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "command/command_context.h"
#include "command/command_image_support.h"
#include "protobuf/protobuf_util.h"

using namespace protobuf_util;

TEST(CommandImageSupportTest, SetGetDepth)
{
    CommandImageSupport &image = CommandImageSupport::Instance();

    image.SetDepth(1);
    EXPECT_EQ(1, image.GetDepth());
}

TEST(CommandImageSupportTest, SetGetDefaultFolder)
{
    CommandImageSupport &image = CommandImageSupport::Instance();

    EXPECT_NE(string::npos, image.GetDefaultFolder().find("/images"));

    EXPECT_FALSE(image.SetDefaultFolder("").empty());
    EXPECT_FALSE(image.SetDefaultFolder("/not_in_home").empty());
}

TEST(CommandImageSupportTest, CreateImage)
{
    const CommandImageSupport &image = CommandImageSupport::Instance();

    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1);
    EXPECT_FALSE(image.CreateImage(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2);
    EXPECT_FALSE(image.CreateImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "file", "filename");
    SetParam(command3, "size", "-1");
    CommandContext context3(command3);
    EXPECT_FALSE(image.CreateImage(context3)) << "Size must be reported as invalid";

    PbCommand command4;
    SetParam(command4, "file", "filename");
    SetParam(command4, "size", "");
    CommandContext context4(command4);
    EXPECT_FALSE(image.CreateImage(context4)) << "Size must be reported as missing";

    PbCommand command5;
    SetParam(command5, "size", "1");
    CommandContext context5(command5);
    EXPECT_FALSE(image.CreateImage(context5)) << "Size must be reported as invalid";

    PbCommand command6;
    SetParam(command6, "size", "513");
    CommandContext context6(command6);
    EXPECT_FALSE(image.CreateImage(context6)) << "Size must be reported as not a multiple of 512";

    // Further tests would modify the filesystem
}

TEST(CommandImageSupportTest, DeleteImage)
{
    const CommandImageSupport &image = CommandImageSupport::Instance();

    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1);
    EXPECT_FALSE(image.DeleteImage(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2);
    EXPECT_FALSE(image.DeleteImage(context2)) << "Depth must be reported as invalid";

    MockStorageDevice device;
    device.SetFilename("filename");
    device.ReserveFile();
    PbCommand command3;
    SetParam(command3, "file", "filename");
    CommandContext context3(command3);
    EXPECT_FALSE(image.DeleteImage(context3)) << "File must be reported as in use";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, RenameImage)
{
    const CommandImageSupport &image = CommandImageSupport::Instance();

    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1);
    EXPECT_FALSE(image.RenameImage(context1)) << "Source filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "from", "/a/b/c/filename_from");
    CommandContext context2(command2);
    EXPECT_FALSE(image.RenameImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "from", "filename_from");
    CommandContext context3(command3);
    EXPECT_FALSE(image.RenameImage(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, CopyImage)
{
    const CommandImageSupport &image = CommandImageSupport::Instance();

    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1);
    EXPECT_FALSE(image.CopyImage(context1)) << "Source filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "from", "/a/b/c/filename_from");
    CommandContext context2(command2);
    EXPECT_FALSE(image.CopyImage(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "from", "filename_from");
    CommandContext context3(command3);
    EXPECT_FALSE(image.CopyImage(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}

TEST(CommandImageSupportTest, SetImagePermissions)
{
    const CommandImageSupport &image = CommandImageSupport::Instance();

    StorageDevice::SetReservedFiles( { });

    PbCommand command1;
    CommandContext context1(command1);
    EXPECT_FALSE(image.SetImagePermissions(context1)) << "Filename must be reported as missing";

    PbCommand command2;
    SetParam(command2, "file", "/a/b/c/filename");
    CommandContext context2(command2);
    EXPECT_FALSE(image.SetImagePermissions(context2)) << "Depth must be reported as invalid";

    PbCommand command3;
    SetParam(command3, "file", "filename");
    CommandContext context3(command3);
    EXPECT_FALSE(image.CopyImage(context3)) << "Source file must be reported as missing";

    // Further testing would modify the filesystem
}
