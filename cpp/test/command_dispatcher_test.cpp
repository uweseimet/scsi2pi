//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "protobuf/protobuf_util.h"
#include "command/command_dispatcher.h"

using namespace protobuf_util;

TEST(CommandDispatcherTest, DispatchCommand)
{
    auto bus = make_shared<MockBus>();
    auto controller_factory = make_shared<ControllerFactory>(false);
    MockCommandExecutor executor(*bus, controller_factory);
    S2pImage image;
    CommandDispatcher dispatcher(image, executor);
    PbResult result;

    PbCommand command_log_level;
    command_log_level.set_operation(PbOperation::LOG_LEVEL);
    SetParam(command_log_level, "level", "invalid");
    CommandContext context_log_level(command_log_level, "", "");
    EXPECT_FALSE(dispatcher.DispatchCommand(context_log_level, result, ""));
    EXPECT_FALSE(result.status());

    PbCommand command_devices_info;
    command_devices_info.set_operation(PbOperation::DEVICES_INFO);
    CommandContext context_devices_info(command_devices_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_devices_info, result, ""));
    EXPECT_TRUE(result.status());

    PbCommand command_device_types_info;
    command_device_types_info.set_operation(PbOperation::DEVICE_TYPES_INFO);
    CommandContext context_device_types_info(command_device_types_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_device_types_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &device_types_info = result.device_types_info();
    EXPECT_NE(0, device_types_info.properties().size());

    PbCommand command_server_info;
    command_server_info.set_operation(PbOperation::SERVER_INFO);
    CommandContext context_server_info(command_server_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_server_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &server_info = result.server_info();
    EXPECT_TRUE(server_info.has_version_info());

    PbCommand command_version_info;
    command_version_info.set_operation(PbOperation::VERSION_INFO);
    CommandContext context_version_info(command_version_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_version_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &version_info = result.version_info();
    EXPECT_NE(0, version_info.major_version());

    PbCommand command_log_level_info;
    command_log_level_info.set_operation(PbOperation::LOG_LEVEL_INFO);
    CommandContext context_log_level_info(command_log_level_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_log_level_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &log_level_info = result.log_level_info();
    EXPECT_NE(0, log_level_info.log_levels_size());

    PbCommand command_network_interfaces_info;
    command_network_interfaces_info.set_operation(PbOperation::NETWORK_INTERFACES_INFO);
    CommandContext context_network_interfaces_info(command_network_interfaces_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_network_interfaces_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &network_interfaces_info = result.network_interfaces_info();
    EXPECT_NE(0, network_interfaces_info.name_size());

    PbCommand command_mapping_info;
    command_mapping_info.set_operation(PbOperation::MAPPING_INFO);
    CommandContext context_mapping_info(command_mapping_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_mapping_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &mapping_info = result.mapping_info();
    EXPECT_NE(0, mapping_info.mapping_size());

    PbCommand command_statistics_info;
    command_statistics_info.set_operation(PbOperation::STATISTICS_INFO);
    CommandContext context_statistics_info(command_statistics_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_statistics_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &statistics_info = result.statistics_info();
    EXPECT_EQ(0, statistics_info.statistics_size());

    PbCommand command_properties_info;
    command_properties_info.set_operation(PbOperation::PROPERTIES_INFO);
    CommandContext context_properties_info(command_properties_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_properties_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &properties_info = result.properties_info();
    EXPECT_EQ(0, properties_info.s2p_properties_size());

    PbCommand command_operation_info;
    command_operation_info.set_operation(PbOperation::OPERATION_INFO);
    CommandContext context_operation_info(command_operation_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_operation_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &operation_info = result.operation_info();
    EXPECT_NE(0, operation_info.operations_size());

    PbCommand command_reserved_ids_info;
    command_reserved_ids_info.set_operation(PbOperation::RESERVED_IDS_INFO);
    CommandContext context_reserved_ids_info(command_reserved_ids_info, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_reserved_ids_info, result, ""));
    EXPECT_TRUE(result.status());
    const auto &reserved_ids_info = result.reserved_ids_info();
    EXPECT_EQ(0, reserved_ids_info.ids_size());

    PbCommand command_no_operation;
    command_no_operation.set_operation(PbOperation::NO_OPERATION);
    CommandContext context_no_operation(command_no_operation, "", "");
    EXPECT_TRUE(dispatcher.DispatchCommand(context_no_operation, result, ""));
    EXPECT_TRUE(result.status());
}
