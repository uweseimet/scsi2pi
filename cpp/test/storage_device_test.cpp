//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include "mocks.h"
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "devices/storage_device.h"

using namespace filesystem;

TEST(StorageDeviceTest, SetGetFilename)
{
    MockStorageDevice device;

    device.SetFilename("filename");
    EXPECT_EQ("filename", device.GetFilename());
}

TEST(StorageDeviceTest, ValidateFile)
{
    MockStorageDevice device;

    device.SetBlockCount(0);
    device.SetFilename("/non_existing_file");
    EXPECT_THROW(device.ValidateFile(), io_exception);

    device.SetBlockCount(1);
    EXPECT_THROW(device.ValidateFile(), io_exception);

    const path filename = CreateTempFile(1);
    device.SetFilename(string(filename));
    device.SetReadOnly(false);
    device.SetProtectable(true);
    device.ValidateFile();
    EXPECT_FALSE(device.IsReadOnly());
    EXPECT_TRUE(device.IsProtectable());
    EXPECT_FALSE(device.IsStopped());
    EXPECT_FALSE(device.IsRemoved());
    EXPECT_FALSE(device.IsLocked());

    permissions(filename, perms::owner_read);
    device.SetReadOnly(false);
    device.SetProtectable(true);
    device.ValidateFile();
    EXPECT_TRUE(device.IsReadOnly());
    EXPECT_FALSE(device.IsProtectable());
    EXPECT_FALSE(device.IsProtected());
    EXPECT_FALSE(device.IsStopped());
    EXPECT_FALSE(device.IsRemoved());
    EXPECT_FALSE(device.IsLocked());
}

TEST(StorageDeviceTest, MediumChanged)
{
    MockStorageDevice device;

    EXPECT_FALSE(device.IsMediumChanged());

    device.SetMediumChanged(true);
    EXPECT_TRUE(device.IsMediumChanged());

    device.SetMediumChanged(false);
    EXPECT_FALSE(device.IsMediumChanged());
}

TEST(StorageDeviceTest, GetIdsForReservedFile)
{
    const int ID = 1;
    const int LUN = 0;
    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    MockAbstractController controller(bus, ID);
    auto device = make_shared<MockScsiHd>(LUN, false);
    device->SetFilename("filename");
    StorageDevice::SetReservedFiles( { });

    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device));

    const auto [id1, lun1] = StorageDevice::GetIdsForReservedFile("filename");
    EXPECT_EQ(-1, id1);
    EXPECT_EQ(-1, lun1);

    device->ReserveFile();
    const auto [id2, lun2] = StorageDevice::GetIdsForReservedFile("filename");
    EXPECT_EQ(ID, id2);
    EXPECT_EQ(LUN, lun2);

    device->UnreserveFile();
    const auto [id3, lun3] = StorageDevice::GetIdsForReservedFile("filename");
    EXPECT_EQ(-1, id3);
    EXPECT_EQ(-1, lun3);
}

TEST(StorageDeviceTest, GetSetReservedFiles)
{
    const int ID = 1;
    const int LUN = 0;
    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    MockAbstractController controller(bus, ID);
    auto device = make_shared<MockScsiHd>(LUN, false);
    device->SetFilename("filename");

    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device));

    device->ReserveFile();

    const auto &reserved_files = StorageDevice::GetReservedFiles();
    EXPECT_EQ(1U, reserved_files.size());
    EXPECT_TRUE(reserved_files.contains("filename"));

    StorageDevice::SetReservedFiles(reserved_files);
    EXPECT_EQ(1U, reserved_files.size());
    EXPECT_TRUE(reserved_files.contains("filename"));
}

TEST(StorageDeviceTest, FileExists)
{
    EXPECT_FALSE(StorageDevice::FileExists("/non_existing_file"));
    EXPECT_TRUE(StorageDevice::FileExists("/dev/null"));
}

TEST(StorageDeviceTest, GetFileSize)
{
    MockStorageDevice device;

    const path filename = CreateTempFile(512);
    device.SetFilename(filename.c_str());
    const off_t size = device.GetFileSize();
    EXPECT_EQ(512, size);

    device.SetFilename("/non_existing_file");
    EXPECT_THROW(device.GetFileSize(), io_exception);
}
