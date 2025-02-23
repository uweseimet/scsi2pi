//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/device_factory.h"
#include "command/command_image_support.h"
#include "command/command_response.h"
#include "controllers/controller_factory.h"
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_version.h"

using namespace spdlog;
using namespace command_response;
using namespace s2p_interface_util;

TEST(CommandResponseTest, Operation_Count)
{
    PbOperationInfo info;
    GetOperationInfo(info);
    EXPECT_EQ(34, info.operations_size());
}

void TestNonDiskDevice(PbDeviceType type, unsigned int default_param_count)
{
    MockBus bus;
    ControllerFactory controller_factory;

    auto d = DeviceFactory::GetInstance().CreateDevice(type, 0, "");
    const param_map params;
    d->Init();
    EXPECT_TRUE(controller_factory.AttachToController(bus, 0, d));

    PbServerInfo info;
    GetDevices(controller_factory.GetAllDevices(), info);

    EXPECT_EQ(1, info.devices_info().devices().size());

    const auto &device = info.devices_info().devices()[0];
    EXPECT_FALSE(device.properties().read_only());
    EXPECT_FALSE(device.properties().protectable());
    EXPECT_FALSE(device.properties().stoppable());
    EXPECT_FALSE(device.properties().removable());
    EXPECT_FALSE(device.properties().lockable());
    EXPECT_EQ(32, device.properties().luns());
    EXPECT_EQ(0, device.block_size());
    EXPECT_EQ(0U, device.block_count());
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
    PbImageFile image_file;

    EXPECT_FALSE(GetImageFile(image_file, ""));

    // Even though the call fails (non-existing file) some properties must be set
    EXPECT_FALSE(GetImageFile(image_file, "filename.hds"));
    EXPECT_EQ("filename.hds", image_file.name());
    EXPECT_EQ(SCHD, image_file.type());
}

TEST(CommandResponseTest, GetReservedIds)
{
    unordered_set<int> ids;

    PbReservedIdsInfo info1;
    GetReservedIds(info1, ids);
    EXPECT_TRUE(info1.ids().empty());

    ids.insert(3);
    PbReservedIdsInfo info2;
    GetReservedIds(info2, ids);
    EXPECT_EQ(1, info2.ids().size());
    EXPECT_EQ(3, info2.ids()[0]);
}

TEST(CommandResponseTest, GetDevicesInfo)
{
    const int ID = 2;
    const int LUN1 = 0;
    const int LUN2 = 5;
    const int LUN3 = 6;

    MockBus bus;
    ControllerFactory controller_factory;
    PbCommand command;

    PbResult result1;
    GetDevicesInfo(controller_factory.GetAllDevices(), result1, command);
    EXPECT_TRUE(result1.status());
    EXPECT_TRUE(result1.devices_info().devices().empty());

    auto device1 = make_shared<MockHostServices>(LUN1);
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device1));

    GetDevicesInfo(controller_factory.GetAllDevices(), result1, command);
    EXPECT_TRUE(result1.status());
    auto &devices1 = result1.devices_info().devices();
    EXPECT_EQ(1, devices1.size());
    EXPECT_EQ(SCHS, devices1[0].type());
    EXPECT_EQ(ID, devices1[0].id());
    EXPECT_EQ(LUN1, devices1[0].unit());

    auto device2 = make_shared<MockScsiHd>(LUN2, false);
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device2));

    PbResult result2;
    GetDevicesInfo(controller_factory.GetAllDevices(), result2, command);
    EXPECT_TRUE(result2.status());
    auto &devices2 = result2.devices_info().devices();
    EXPECT_EQ(2, devices2.size()) << "Device count mismatch";

    auto *requested_device = command.add_devices();
    requested_device->set_id(ID);
    requested_device->set_unit(LUN1);
    PbResult result3;
    GetDevicesInfo(controller_factory.GetAllDevices(), result3, command);
    EXPECT_TRUE(result3.status());
    auto &devices3 = result3.devices_info().devices();
    EXPECT_EQ(1, devices3.size()) << "Only data for the specified ID and LUN must be returned";
    EXPECT_EQ(SCHS, devices3[0].type());
    EXPECT_EQ(ID, devices3[0].id());
    EXPECT_EQ(LUN1, devices3[0].unit());

    requested_device->set_id(ID);
    requested_device->set_unit(LUN3);
    PbResult result4;
    GetDevicesInfo(controller_factory.GetAllDevices(), result4, command);
    EXPECT_FALSE(result4.status()) << "Only data for the specified ID and LUN must be returned";
}

TEST(CommandResponseTest, GetDeviceTypesInfo)
{
    PbDeviceTypesInfo info;
    GetDeviceTypesInfo(info);
#ifdef __linux__
    EXPECT_EQ(10, info.properties().size());
#else
    EXPECT_EQ(9, info.properties().size());
#endif
}

TEST(CommandResponseTest, GetServerInfo)
{
    const unordered_set<shared_ptr<PrimaryDevice>> devices;
    const unordered_set<int> ids = { 1, 3 };

    PbCommand command;
    PbServerInfo info1;
    CommandImageSupport::GetInstance().SetDepth(1234);

    GetServerInfo(info1, command, devices, ids, *default_logger());
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
    EXPECT_EQ(1234, info1.image_files_info().depth());
    EXPECT_EQ(2, info1.reserved_ids_info().ids().size());

    SetParam(command, "operations", "log_level_info,mapping_info");
    PbServerInfo info2;
    GetServerInfo(info2, command, devices, ids, *default_logger());
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
    PbVersionInfo info;
    GetVersionInfo(info);
    EXPECT_EQ(s2p_major_version, info.major_version());
    EXPECT_EQ(s2p_minor_version, info.minor_version());
    EXPECT_EQ(s2p_revision, info.patch_version());
    EXPECT_EQ(s2p_suffix, info.suffix());
}

TEST(CommandResponseTest, GetLogLevelInfo)
{
    PbLogLevelInfo info;
    GetLogLevelInfo(info);
    EXPECT_EQ(level::level_string_views[get_level()], info.current_log_level());
    EXPECT_EQ(7, info.log_levels().size());
}

#ifdef __linux__
TEST(CommandResponseTest, GetNetworkInterfacesInfo)
{
    PbNetworkInterfacesInfo info;
    GetNetworkInterfacesInfo(info);
    EXPECT_FALSE(info.name().empty());
}
#endif

TEST(CommandResponseTest, GetMappingInfo)
{
    PbMappingInfo info;
    GetMappingInfo(info);
    EXPECT_EQ(11U, info.mapping().size());
}

TEST(CommandResponseTest, GetStatisticsInfo)
{
    unordered_set<shared_ptr<PrimaryDevice>> devices;

    PbStatisticsInfo info;
    devices.insert(make_shared<MockPrimaryDevice>(0));
    GetStatisticsInfo(info, devices);
    EXPECT_EQ(0, info.statistics().size());

    devices.insert(make_shared<MockScsiHd>(0, false));
    GetStatisticsInfo(info, devices);
    const auto &statistics = info.statistics();
    EXPECT_EQ(2, statistics.size());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics.Get(0).category());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics.Get(1).category());
    EXPECT_EQ(0, statistics.Get(0).unit());
    EXPECT_EQ(0, statistics.Get(1).unit());
    EXPECT_EQ(0U, statistics.Get(0).value());
    EXPECT_EQ(0U, statistics.Get(1).value());
}
