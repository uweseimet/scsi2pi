//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/storage_device.h"
#include "shared/s2p_exceptions.h"

pair<shared_ptr<MockAbstractController>, shared_ptr<MockStorageDevice>> CreateStorageDevice()
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(0);
    auto device = make_shared<MockStorageDevice>();
    EXPECT_TRUE(device->Init( { }));
    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

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
    const auto &filename = CreateTempFile(1);
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

TEST(StorageDeviceTest, CheckWritePreconditions)
{
    MockStorageDevice device;
    device.SetProtectable(true);

    device.SetProtected(false);
    EXPECT_NO_THROW(device.CheckWritePreconditions());

    device.SetProtected(true);
    EXPECT_THROW(device.CheckWritePreconditions(), scsi_exception);
}

TEST(StorageDeviceTest, PreventAllowMediumRemoval)
{
    auto [controller, device] = CreateStorageDevice();

    TestShared::Dispatch(*device, scsi_command::prevent_allow_medium_removal, sense_key::not_ready,
        asc::medium_not_present, "PREVENT/ALLOW MEDIUM REMOVAL must fail because device is not ready");

    device->SetReady(true);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::prevent_allow_medium_removal));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_FALSE(device->IsLocked());

    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::prevent_allow_medium_removal));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_TRUE(device->IsLocked());
}

TEST(StorageDeviceTest, StartStopUnit)
{
    auto [controller, device] = CreateStorageDevice();

    device->SetRemovable(true);

    // Stop/Unload
    device->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_TRUE(device->IsStopped());

    // Stop/Load
    controller->SetCdbByte(4, 0x02);
    device->SetReady(true);
    device->SetLocked(false);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    device->SetReady(false);
    TestShared::Dispatch(*device, scsi_command::start_stop, sense_key::illegal_request,
        asc::load_or_eject_failed, "START/STOP must fail because device is not ready");

    // Stop/Load
    controller->SetCdbByte(4, 0x02);
    device->SetReady(true);
    device->SetLocked(true);
    TestShared::Dispatch(*device, scsi_command::start_stop, sense_key::illegal_request,
        asc::load_or_eject_failed, "LOAD/EJECT must fail because device is locked");

    // Start/Unload
    controller->SetCdbByte(4, 0x01);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_FALSE(device->IsStopped());

    // Start/Load
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // Start/Load with previous medium
    controller->SetCdbByte(4, 0x02);
    device->SetLocked(false);
    device->SetFilename("filename");
    EXPECT_TRUE(device->GetLastFilename().empty());
    EXPECT_CALL(*controller, Status);
    // Eject existing medium
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_TRUE(device->GetFilename().empty());
    EXPECT_EQ("filename", device->GetLastFilename());
    // Re-load medium
    device->SetFilename("filename");
    device->ReserveFile();
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status).Times(0);
    EXPECT_THROW(device->Dispatch(scsi_command::start_stop), scsi_exception)
    << "Filename is already reserved";
    device->UnreserveFile();
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ("filename", device->GetFilename());
}

TEST(StorageDeviceTest, Eject)
{
    MockStorageDevice device;

    device.SetReady(false);
    device.SetRemovable(false);
    device.SetLocked(false);
    EXPECT_FALSE(device.Eject(false));

    device.SetRemovable(true);
    EXPECT_FALSE(device.Eject(false));

    device.SetReady(true);
    device.SetLocked(true);
    EXPECT_FALSE(device.Eject(false));

    device.SetReady(true);
    device.SetLocked(false);
    EXPECT_TRUE(device.Eject(false));

    device.SetReady(true);
    EXPECT_TRUE(device.Eject(true));
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
    MockStorageDevice device;

    EXPECT_TRUE(device.SetConfiguredBlockSize(512));
    EXPECT_EQ(512U, device.GetConfiguredBlockSize());

    EXPECT_FALSE(device.SetConfiguredBlockSize(4));
    EXPECT_EQ(512U, device.GetConfiguredBlockSize());

    EXPECT_FALSE(device.SetConfiguredBlockSize(1234));
    EXPECT_EQ(512U, device.GetConfiguredBlockSize());
}

TEST(StorageDeviceTest, SetBlockSize)
{
    MockStorageDevice device;

    EXPECT_TRUE(device.SetBlockSize(512));
    EXPECT_FALSE(device.SetBlockSize(520));
}

TEST(StorageDeviceTest, ValidateBlockSize)
{
    MockStorageDevice device;

    EXPECT_FALSE(device.ValidateBlockSize(0));
    EXPECT_FALSE(device.ValidateBlockSize(4));
    EXPECT_FALSE(device.ValidateBlockSize(7));
    EXPECT_TRUE(device.ValidateBlockSize(512));
    EXPECT_FALSE(device.ValidateBlockSize(131072));
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
    EXPECT_EQ(512, device.GetFileSize());

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

    device.ChangeBlockSize(1024);
    EXPECT_EQ(1024U, device.GetBlockSize());

    EXPECT_THROW(device.ChangeBlockSize(513), scsi_exception);
    EXPECT_EQ(1024U, device.GetBlockSize());

    device.ChangeBlockSize(512);
    EXPECT_EQ(512U, device.GetBlockSize());
}

TEST(StorageDeviceTest, EvaluateBlockDescriptors)
{
    MockStorageDevice device;

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::mode_select_6, {}, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::mode_select_6,
                CreateParameters("00:00:00:ff:00:00:00:00:00:00:08:00"), 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::mode_select_10, {}, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));

    EXPECT_THAT([&] {device.EvaluateBlockDescriptors(scsi_command::mode_select_10,
                CreateParameters("00:00:00:00:00:00:00:ff:00:08:00:00:00:00:00:00"), 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));

    pair<int, int> result;

    EXPECT_NO_THROW(
        result = device.EvaluateBlockDescriptors(scsi_command::mode_select_6,
            CreateParameters("00:00:00:04:00:00:00:00:00:00:08:00"), 512));
    EXPECT_EQ(8, result.first);
    EXPECT_EQ(2048, result.second);

    EXPECT_NO_THROW(
        result = device.EvaluateBlockDescriptors(scsi_command::mode_select_6,
            CreateParameters("00:00:00:04:00:00:00:00:00:00:08:04"), result.second));
    EXPECT_EQ(8, result.first);
    EXPECT_EQ(2052, result.second);

    EXPECT_NO_THROW(
        result = device.EvaluateBlockDescriptors(scsi_command::mode_select_10,
            CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:04:00"), result.second));
    EXPECT_EQ(16, result.first);
    EXPECT_EQ(1024, result.second);

    EXPECT_NO_THROW(
        result = device.EvaluateBlockDescriptors(scsi_command::mode_select_10,
            CreateParameters("00:00:00:00:00:00:00:08:00:08:00:00:00:00:03:fc"), result.second));
    EXPECT_EQ(16, result.first);
    EXPECT_EQ(1020, result.second);
}

TEST(StorageDeviceTest, VerifyBlockSizeChange)
{
    MockStorageDevice device;
    device.SetBlockSize(512);

    EXPECT_EQ(512, device.VerifyBlockSizeChange(512, false));

    EXPECT_EQ(1024, device.VerifyBlockSizeChange(1024, true));

    EXPECT_THAT([&] {device.VerifyBlockSizeChange(2048, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));

    EXPECT_THAT([&] {device.VerifyBlockSizeChange(0, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(513, false);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(0, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
    EXPECT_THAT([&] {device.VerifyBlockSizeChange(513, true);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
        Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
}

TEST(StorageDeviceTest, ModeSense6)
{
    auto [controller, device] = CreateStorageDevice();

    // Drive must be ready in order to return all data
    device->SetReady(true);

    controller->SetCdbByte(2, 0x3f);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);

    device->SetBlockCount(0x00000001);
    device->SetBlockSize(1024);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_6));
    EXPECT_EQ(8, controller->GetBuffer()[3]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00000001U, GetInt32(controller->GetBuffer(), 4)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block size";

    device->SetBlockCount(0xffffffff);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_6));
    EXPECT_EQ(0xffffffff, GetInt32(controller->GetBuffer(), 4)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block size";

    device->SetBlockCount(0x100000000);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_6));
    EXPECT_EQ(0xffffffff, GetInt32(controller->GetBuffer(), 4)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block size";

    // No block descriptor
    controller->SetCdbByte(1, 0x08);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_6));
    EXPECT_EQ(0x00, controller->GetBuffer()[2]) << "Wrong device-specific parameter";

    device->SetReadOnly(false);
    device->SetProtectable(true);
    device->SetProtected(true);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_6));
    const auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x80, buf[2]) << "Wrong device-specific parameter";

    controller->SetCdbByte(3, 0x01);
    EXPECT_THROW(device->Dispatch(scsi_command::mode_sense_6), scsi_exception)<< "Subpages are not supported";
}

TEST(StorageDeviceTest, ModeSense10)
{
    auto [controller, device] = CreateStorageDevice();

    // Drive must be ready in order to return all data
    device->SetReady(true);

    controller->SetCdbByte(2, 0x3f);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);

    device->SetBlockCount(0x00000001);
    device->SetBlockSize(1024);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    EXPECT_EQ(8, controller->GetBuffer()[7]) << "Wrong block descriptor length";
    EXPECT_EQ(0x00000001U, GetInt32(controller->GetBuffer(), 8)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 12)) << "Wrong block size";

    device->SetBlockCount(0xffffffff);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    EXPECT_EQ(0xffffffff, GetInt32(controller->GetBuffer(), 8)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 12)) << "Wrong block size";

    device->SetBlockCount(0x100000000);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    EXPECT_EQ(0xffffffff, GetInt32(controller->GetBuffer(), 8)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 12)) << "Wrong block size";

    // LLBAA
    controller->SetCdbByte(1, 0x10);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    EXPECT_EQ(0x100000000U, GetInt64(controller->GetBuffer(), 8)) << "Wrong block count";
    EXPECT_EQ(1024U, GetInt32(controller->GetBuffer(), 20)) << "Wrong block size";
    EXPECT_EQ(0x01, controller->GetBuffer()[4]) << "LLBAA is not set";

    // No block descriptor
    controller->SetCdbByte(1, 0x08);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    auto &buf = controller->GetBuffer();
    EXPECT_EQ(0x00, controller->GetBuffer()[3]) << "Wrong device-specific parameter";

    device->SetReadOnly(false);
    device->SetProtectable(true);
    device->SetProtected(true);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::mode_sense_10));
    buf = controller->GetBuffer();
    EXPECT_EQ(0x80, buf[3]) << "Wrong device-specific parameter";

    controller->SetCdbByte(3, 0x01);
    EXPECT_THROW(device->Dispatch(scsi_command::mode_sense_10), scsi_exception)<< "Subpages are not supported";
}

TEST(StorageDeviceTest, GetStatistics)
{
    MockStorageDevice device;

    auto statistics = device.GetStatistics();
    EXPECT_EQ(2U, statistics.size());
    EXPECT_EQ("block_read_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ("block_write_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());

    device.UpdateReadCount(1);
    device.UpdateWriteCount(2);
    statistics = device.GetStatistics();
    EXPECT_EQ(2U, statistics.size());
    EXPECT_EQ("block_read_count", statistics[0].key());
    EXPECT_EQ(1U, statistics[0].value());
    EXPECT_EQ("block_write_count", statistics[1].key());
    EXPECT_EQ(2U, statistics[1].value());
}
