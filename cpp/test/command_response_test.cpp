//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_version.h"
#include "shared_command/command_response.h"
#include "shared_protobuf/protobuf_util.h"
#include "controllers/controller_factory.h"
#include "base/device_factory.h"

using namespace s2p_interface;
using namespace spdlog;
using namespace protobuf_util;

TEST(CommandResponseTest, Operation_Count)
{
    CommandResponse response;

    PbOperationInfo info;
    response.GetOperationInfo(info, 0);
    EXPECT_EQ(PbOperation_ARRAYSIZE - 1, info.operations_size());
}

void TestNonDiskDevice(PbDeviceType type, int default_param_count)
{
    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    DeviceFactory device_factory;
    CommandResponse response;

    auto d = device_factory.CreateDevice(type, 0, "");
    const param_map params;
    d->Init(params);
    EXPECT_TRUE(controller_factory.AttachToController(*bus, 0, d));

    PbServerInfo info;
    response.GetDevices(controller_factory.GetAllDevices(), info, "image_folder");

    EXPECT_EQ(1, info.devices_info().devices().size());

    const auto &device = info.devices_info().devices()[0];
    EXPECT_FALSE(device.properties().read_only());
    EXPECT_FALSE(device.properties().protectable());
    EXPECT_FALSE(device.properties().stoppable());
    EXPECT_FALSE(device.properties().removable());
    EXPECT_FALSE(device.properties().lockable());
    EXPECT_EQ(32, device.properties().luns());
    EXPECT_EQ(0, device.block_size());
    EXPECT_EQ(0, device.block_count());
    EXPECT_EQ(default_param_count, device.properties().default_params().size());
    EXPECT_FALSE(device.properties().supports_file());
    if (default_param_count) {
        EXPECT_TRUE(device.properties().supports_params());
    }
    else {
        EXPECT_FALSE(device.properties().supports_params());
    }
}

TEST(CommandResponseTest, GetDevices)
{
    TestNonDiskDevice(SCHS, 0);
    TestNonDiskDevice(SCLP, 1);
}

TEST(CommandResponseTest, GetImageFile)
{
    CommandResponse response;
    PbImageFile image_file;

    EXPECT_FALSE(response.GetImageFile(image_file, "default_folder", ""));

    // Even though the call fails (non-existing file) some properties must be set
    EXPECT_FALSE(response.GetImageFile(image_file, "default_folder", "filename.hds"));
    EXPECT_EQ("filename.hds", image_file.name());
    EXPECT_EQ(SCHD, image_file.type());
}

TEST(CommandResponseTest, GetImageFilesInfo)
{
    CommandResponse response;

    PbImageFilesInfo info;
    response.GetImageFilesInfo(info, "default_folder", "", "", 1);
    EXPECT_TRUE(info.image_files().empty());
}

TEST(CommandResponseTest, GetReservedIds)
{
    CommandResponse response;
    unordered_set<int> ids;

    PbReservedIdsInfo info1;
    response.GetReservedIds(info1, ids);
    EXPECT_TRUE(info1.ids().empty());

    ids.insert(3);
    PbReservedIdsInfo info2;
    response.GetReservedIds(info2, ids);
    EXPECT_EQ(1, info2.ids().size());
    EXPECT_EQ(3, info2.ids()[0]);
}

TEST(CommandResponseTest, GetDevicesInfo)
{
    const int ID = 2;
    const int LUN1 = 0;
    const int LUN2 = 5;
    const int LUN3 = 6;

    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    CommandResponse response;
    PbCommand command;

    PbResult result1;
    response.GetDevicesInfo(controller_factory.GetAllDevices(), result1, command, "");
    EXPECT_TRUE(result1.status());
    EXPECT_TRUE(result1.devices_info().devices().empty());

    auto device1 = make_shared<MockHostServices>(LUN1);
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device1));

    response.GetDevicesInfo(controller_factory.GetAllDevices(), result1, command, "");
    EXPECT_TRUE(result1.status());
    auto &devices1 = result1.devices_info().devices();
    EXPECT_EQ(1, devices1.size());
    EXPECT_EQ(SCHS, devices1[0].type());
    EXPECT_EQ(ID, devices1[0].id());
    EXPECT_EQ(LUN1, devices1[0].unit());

    auto device2 = make_shared<MockScsiHd>(LUN2, false);
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device2));

    PbResult result2;
    response.GetDevicesInfo(controller_factory.GetAllDevices(), result2, command, "");
    EXPECT_TRUE(result2.status());
    auto &devices2 = result2.devices_info().devices();
    EXPECT_EQ(2, devices2.size()) << "Device count mismatch";

    auto requested_device = command.add_devices();
    requested_device->set_id(ID);
    requested_device->set_unit(LUN1);
    PbResult result3;
    response.GetDevicesInfo(controller_factory.GetAllDevices(), result3, command, "");
    EXPECT_TRUE(result3.status());
    auto &devices3 = result3.devices_info().devices();
    EXPECT_EQ(1, devices3.size()) << "Only data for the specified ID and LUN must be returned";
    EXPECT_EQ(SCHS, devices3[0].type());
    EXPECT_EQ(ID, devices3[0].id());
    EXPECT_EQ(LUN1, devices3[0].unit());

    requested_device->set_id(ID);
    requested_device->set_unit(LUN3);
    PbResult result4;
    response.GetDevicesInfo(controller_factory.GetAllDevices(), result4, command, "");
    EXPECT_FALSE(result4.status()) << "Only data for the specified ID and LUN must be returned";
}

TEST(CommandResponseTest, GetDeviceTypesInfo)
{
    CommandResponse response;

    PbDeviceTypesInfo info;
    response.GetDeviceTypesInfo(info);
    EXPECT_EQ(8, info.properties().size());
}

TEST(CommandResponseTest, GetServerInfo)
{
    auto bus = make_shared<MockBus>();
    CommandResponse response;
    const unordered_set<shared_ptr<PrimaryDevice>> devices;
    const unordered_set<int> ids = { 1, 3 };

    PbCommand command;
    PbServerInfo info1;
    response.GetServerInfo(info1, command, devices, ids, "default_folder", 1234);
    EXPECT_TRUE(info1.has_version_info());
    EXPECT_TRUE(info1.has_log_level_info());
    EXPECT_TRUE(info1.has_device_types_info());
    EXPECT_TRUE(info1.has_image_files_info());
    EXPECT_TRUE(info1.has_network_interfaces_info());
    EXPECT_TRUE(info1.has_mapping_info());
    EXPECT_TRUE(info1.has_statistics_info());
    EXPECT_FALSE(info1.has_devices_info());
    EXPECT_TRUE(info1.has_reserved_ids_info());
    EXPECT_TRUE(info1.has_operation_info());

    EXPECT_EQ(s2p_major_version, info1.version_info().major_version());
    EXPECT_EQ(s2p_minor_version, info1.version_info().minor_version());
    EXPECT_EQ(s2p_revision, info1.version_info().patch_version());
    EXPECT_EQ(level::level_string_views[get_level()], info1.log_level_info().current_log_level());
    EXPECT_EQ("default_folder", info1.image_files_info().default_image_folder());
    EXPECT_EQ(1234, info1.image_files_info().depth());
    EXPECT_EQ(2, info1.reserved_ids_info().ids().size());

    SetParam(command, "operations", "log_level_info,mapping_info");
    PbServerInfo info2;
    response.GetServerInfo(info2, command, devices, ids, "default_folder", 1234);
    EXPECT_FALSE(info2.has_version_info());
    EXPECT_TRUE(info2.has_log_level_info());
    EXPECT_FALSE(info2.has_device_types_info());
    EXPECT_FALSE(info2.has_image_files_info());
    EXPECT_FALSE(info2.has_network_interfaces_info());
    EXPECT_TRUE(info2.has_mapping_info());
    EXPECT_FALSE(info2.has_statistics_info());
    EXPECT_FALSE(info2.has_devices_info());
    EXPECT_FALSE(info2.has_reserved_ids_info());
    EXPECT_FALSE(info2.has_operation_info());
}

TEST(CommandResponseTest, GetVersionInfo)
{
    CommandResponse response;

    PbVersionInfo info;
    response.GetVersionInfo(info);
    EXPECT_EQ(s2p_major_version, info.major_version());
    EXPECT_EQ(s2p_minor_version, info.minor_version());
    EXPECT_EQ(s2p_revision, info.patch_version());
    EXPECT_EQ(s2p_suffix, info.suffix());
}

TEST(CommandResponseTest, GetLogLevelInfo)
{
    CommandResponse response;

    PbLogLevelInfo info;
    response.GetLogLevelInfo(info);
    EXPECT_EQ(level::level_string_views[get_level()], info.current_log_level());
    EXPECT_EQ(7, info.log_levels().size());
}

#ifdef __linux__
TEST(CommandResponseTest, GetNetworkInterfacesInfo)
{
    CommandResponse response;

    PbNetworkInterfacesInfo info;
    response.GetNetworkInterfacesInfo(info);
    EXPECT_FALSE(info.name().empty());
}
#endif

TEST(CommandResponseTest, GetMappingInfo)
{
    CommandResponse response;

    PbMappingInfo info;
    response.GetMappingInfo(info);
    EXPECT_EQ(7, info.mapping().size());
}
