//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "command/command_context.h"
#include "command/command_dispatcher.h"
#include "protobuf/protobuf_util.h"

using namespace protobuf_util;

TEST(CommandDispatcherTest, DispatchCommand)
{
    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    MockCommandExecutor executor(*bus, controller_factory);
    CommandDispatcher dispatcher(executor);
    PbResult result;

    PbCommand command_invalid;
    command_invalid.set_operation(static_cast<PbOperation>(-1));
    CommandContext context_invalid(command_invalid);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_invalid, result));

    PbCommand command_default_folder;
    command_default_folder.set_operation(DEFAULT_FOLDER);
    SetParam(command_default_folder, "folder", "");
    CommandContext context_default_folder(command_default_folder);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_default_folder, result));

    PbCommand command_log_level;
    command_log_level.set_operation(LOG_LEVEL);
    SetParam(command_log_level, "level", "invalid");
    CommandContext context_log_level1(command_log_level);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_log_level1, result));
    SetParam(command_log_level, "level", "invalid:32");
    CommandContext context_log_level2(command_log_level);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_log_level2, result));

    PbCommand command_devices_info;
    command_devices_info.set_operation(DEVICES_INFO);
    CommandContext context_devices_info(command_devices_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_devices_info, result));

    PbCommand command_device_types_info;
    command_device_types_info.set_operation(DEVICE_TYPES_INFO);
    CommandContext context_device_types_info(command_device_types_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_device_types_info, result));
    const auto &device_types_info = result.device_types_info();
    EXPECT_NE(0, device_types_info.properties().size());

    PbCommand command_server_info;
    command_server_info.set_operation(SERVER_INFO);
    CommandContext context_server_info(command_server_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_server_info, result));
    const auto &server_info = result.server_info();
    EXPECT_TRUE(server_info.has_version_info());

    PbCommand command_version_info;
    command_version_info.set_operation(VERSION_INFO);
    CommandContext context_version_info(command_version_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_version_info, result));
    const auto &version_info = result.version_info();
    EXPECT_NE(0, version_info.major_version());

    PbCommand command_log_level_info;
    command_log_level_info.set_operation(LOG_LEVEL_INFO);
    CommandContext context_log_level_info(command_log_level_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_log_level_info, result));
    const auto &log_level_info = result.log_level_info();
    EXPECT_NE(0, log_level_info.log_levels_size());

    PbCommand command_default_image_files_info;
    command_default_image_files_info.set_operation(DEFAULT_IMAGE_FILES_INFO);
    CommandContext context_default_image_files_info(command_default_image_files_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_default_image_files_info, result));
    const auto &image_files_info = result.image_files_info();
    EXPECT_EQ(1, image_files_info.depth());

    PbCommand image_support_file_info1;
    image_support_file_info1.set_operation(IMAGE_FILE_INFO);
    SetParam(image_support_file_info1, "file", "");
    CommandContext context_image_file_info1(image_support_file_info1);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_image_file_info1, result)) << "Missing filename";
    PbCommand image_support_file_info2;
    image_support_file_info2.set_operation(IMAGE_FILE_INFO);
    SetParam(image_support_file_info2, "file", "invalid");
    CommandContext context_image_file_info2(image_support_file_info2);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_image_file_info2, result)) << "Non-existing file";

#ifdef __linux__
    PbCommand command_network_interfaces_info;
    command_network_interfaces_info.set_operation(NETWORK_INTERFACES_INFO);
    CommandContext context_network_interfaces_info(command_network_interfaces_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_network_interfaces_info, result));
    const auto &network_interfaces_info = result.network_interfaces_info();
    EXPECT_NE(0, network_interfaces_info.name_size());
#endif

    PbCommand command_mapping_info;
    command_mapping_info.set_operation(MAPPING_INFO);
    CommandContext context_mapping_info(command_mapping_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_mapping_info, result));
    const auto &mapping_info = result.mapping_info();
    EXPECT_NE(0, mapping_info.mapping_size());

    PbCommand command_statistics_info;
    command_statistics_info.set_operation(STATISTICS_INFO);
    CommandContext context_statistics_info(command_statistics_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_statistics_info, result));
    const auto &statistics_info = result.statistics_info();
    EXPECT_EQ(0, statistics_info.statistics_size());

    PbCommand command_properties_info;
    command_properties_info.set_operation(PROPERTIES_INFO);
    CommandContext context_properties_info(command_properties_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_properties_info, result));
    const auto &properties_info = result.properties_info();
    EXPECT_EQ(0, properties_info.s2p_properties_size());

    PbCommand command_operation_info;
    command_operation_info.set_operation(OPERATION_INFO);
    CommandContext context_operation_info(command_operation_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_operation_info, result));
    const auto &operation_info = result.operation_info();
    EXPECT_NE(0, operation_info.operations_size());

    PbCommand command_reserved_ids_info;
    command_reserved_ids_info.set_operation(RESERVED_IDS_INFO);
    CommandContext context_reserved_ids_info(command_reserved_ids_info);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_reserved_ids_info, result));
    const auto &reserved_ids_info = result.reserved_ids_info();
    EXPECT_EQ(0, reserved_ids_info.ids_size());

    PbCommand command_shut_down;
    command_shut_down.set_operation(SHUT_DOWN);
    CommandContext context_shut_down(command_shut_down);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_shut_down, result));

    SetParam(command_shut_down, "mode", "rascsi");
    CommandContext context_shut_down_rascsi(command_shut_down);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_shut_down_rascsi, result));

    if (getuid()) {
        SetParam(command_shut_down, "mode", "system");
        CommandContext context_shut_down_system(command_shut_down);
        EXPECT_FALSE(dispatcher.DispatchCommand(context_shut_down_system, result));

        SetParam(command_shut_down, "mode", "reboot");
        CommandContext context_shut_down_reboot(command_shut_down);
        EXPECT_FALSE(dispatcher.DispatchCommand(context_shut_down_reboot, result));
    }

    SetParam(command_shut_down, "mode", "invalid");
    CommandContext context_shut_down_invalid(command_shut_down);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_shut_down_invalid, result));

    PbCommand command_no_operation;
    command_no_operation.set_operation(NO_OPERATION);
    CommandContext context_no_operation(command_no_operation);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_no_operation, result));

    PbCommand command_create_image;
    command_create_image.set_operation(CREATE_IMAGE);
    CommandContext context_create_image(command_create_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_create_image, result));

    PbCommand command_delete_image;
    command_delete_image.set_operation(DELETE_IMAGE);
    CommandContext context_delete_image(command_delete_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_delete_image, result));

    PbCommand command_rename_image;
    command_rename_image.set_operation(RENAME_IMAGE);
    CommandContext context_rename_image(command_rename_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_rename_image, result));

    PbCommand command_copy_image;
    command_copy_image.set_operation(COPY_IMAGE);
    CommandContext context_copy_image(command_copy_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_copy_image, result));

    PbCommand command_protect_image;
    command_protect_image.set_operation(PROTECT_IMAGE);
    CommandContext context_protect_image(command_protect_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_protect_image, result));

    PbCommand command_unprotect_image;
    command_unprotect_image.set_operation(UNPROTECT_IMAGE);
    CommandContext context_unprotect_image(command_unprotect_image);
    EXPECT_FALSE(dispatcher.DispatchCommand(context_unprotect_image, result));

    PbCommand command_reserve_ids;
    command_reserve_ids.set_operation(RESERVE_IDS);
    CommandContext context_reserve_ids(command_reserve_ids);
    EXPECT_TRUE(dispatcher.DispatchCommand(context_reserve_ids, result));
}
