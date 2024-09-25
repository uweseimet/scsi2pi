//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/storage_device.h"
#include "shared/s2p_exceptions.h"

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
    device.SetFilename(filename.string());
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

TEST(StorageDeviceTest, ConfiguredBlockSize)
{
    MockScsiHd device(0, false);

    EXPECT_TRUE(device.SetConfiguredBlockSize(512));
    EXPECT_EQ(512U, device.GetConfiguredBlockSize());

    EXPECT_FALSE(device.SetConfiguredBlockSize(1234));
    EXPECT_EQ(512U, device.GetConfiguredBlockSize());
}

TEST(StorageDeviceTest, SetBlockSizeInBytes)
{
    MockStorageDevice device;

    EXPECT_TRUE(device.SetBlockSizeInBytes(512));
    EXPECT_FALSE(device.SetBlockSizeInBytes(520));
}

TEST(StorageDeviceTest, ReserveUnreserveFile)
{
    MockStorageDevice device1;
    MockStorageDevice device2;

    device1.SetFilename("");
    EXPECT_FALSE(device1.ReserveFile());
    device1.SetFilename("filename1");
    EXPECT_TRUE(device1.ReserveFile());
    EXPECT_FALSE(device1.ReserveFile());
    device2.SetFilename("filename1");
    EXPECT_FALSE(device2.ReserveFile());
    device2.SetFilename("filename2");
    EXPECT_TRUE(device2.ReserveFile());
    device1.UnreserveFile();
    EXPECT_TRUE(device1.GetFilename().empty());
    device2.UnreserveFile();
    EXPECT_TRUE(device2.GetFilename().empty());
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

    const path &filename = CreateTempFile(512);
    device.SetFilename(filename.string());
    const auto &size = device.GetFileSize();
    EXPECT_EQ(512, size);

    device.UnreserveFile();
    device.SetFilename("/non_existing_file");
    EXPECT_THROW(device.GetFileSize(), io_exception);
}

TEST(StorageDeviceTest, BlockCount)
{
    MockStorageDevice device;

    device.SetBlockCount(0x1234567887654321);
    EXPECT_EQ(0x1234567887654321U, device.GetBlockCount());
}

TEST(StorageDeviceTest, ChangeBlockSize)
{
    MockStorageDevice device;

    device.SetBlockSizeInBytes(1024);
    device.ChangeBlockSize(1024);
    EXPECT_EQ(1024U, device.GetBlockSizeInBytes());

    EXPECT_THROW(device.ChangeBlockSize(513), scsi_exception);
    EXPECT_EQ(1024U, device.GetBlockSizeInBytes());

    device.ChangeBlockSize(512);
    EXPECT_EQ(512U, device.GetBlockSizeInBytes());
}

TEST(StorageDeviceTest, EvaluateBlockDescriptors)
{
    vector<uint8_t> buf(14);
    int block_size = 512;
    MockStorageDevice device;

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, {}, block_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Parameter list is too short";
    EXPECT_EQ(512, block_size);

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, {}, block_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Parameter list is too short";
    EXPECT_EQ(512, block_size);

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, {}, block_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Parameter list is too short";
    EXPECT_EQ(512, block_size);

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, {}, block_size);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Parameter list is too short";
    EXPECT_EQ(512, block_size);

    buf = CreateParameters("00:00:00:04:00:00:00:00:00:00:08:00");
    EXPECT_NO_THROW(device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, buf, block_size));
    EXPECT_EQ(2048, block_size);

    buf = CreateParameters("00:00:00:04:00:00:00:00:00:00:08:04");
    EXPECT_NO_THROW(device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, buf, block_size));
    EXPECT_EQ(2052, block_size);

    buf = CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:04:00");
    EXPECT_NO_THROW(device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, buf, block_size));
    EXPECT_EQ(1024, block_size);

    buf = CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:03:fc");
    EXPECT_NO_THROW(device.EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, buf, block_size));
    EXPECT_EQ(1020, block_size);
}

TEST(StorageDeviceTest, VerifyBlockSizeChange)
{
    MockStorageDevice device;
    device.SetBlockSizeInBytes(512);

    EXPECT_EQ(512, device.VerifyBlockSizeChange(512, false));

    EXPECT_EQ(1024, device.VerifyBlockSizeChange(1024, true));

    EXPECT_THAT([&] {device.VerifyBlockSizeChange(2048, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Parameter list is invalid";

    EXPECT_THAT([&] {device.VerifyBlockSizeChange(0, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Parameter list is invalid";
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(513, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Parameter list is invalid";
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(0, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Parameter list is invalid";
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(513, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Parameter list is invalid";
}
