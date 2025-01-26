//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/device_factory.h"
#include "command/command_context.h"
#include "command/command_response.h"
#include "controllers/controller_factory.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"

using namespace protobuf_util;

TEST(CommandExecutorTest, ProcessDeviceCmd)
{
    const int ID = 3;
    const int LUN = 0;

    const auto bus = make_shared<MockBus>();
    MockAbstractController controller(ID);
    ControllerFactory controller_factory;
    const auto executor = make_shared<MockCommandExecutor>(*bus, controller_factory);
    PbDeviceDefinition definition;
    PbCommand command;
    CommandContext context(command, *default_logger());

    definition.set_id(8);
    definition.set_unit(32);
    EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, true)) << "Invalid ID and LUN must fail";

    definition.set_unit(LUN);
    EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, true)) << "Invalid ID must fail";

    definition.set_id(ID);
    definition.set_unit(32);
    EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, true)) << "Invalid LUN must fail";

    definition.set_unit(LUN);
    EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, true)) << "Unknown operation must fail";

    command.set_operation(ATTACH);
    CommandContext context_attach(command, *default_logger());
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_attach, definition, true))
    << "Operation for unknown device type must fail";

    const auto device1 = make_shared<MockPrimaryDevice>(LUN);
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device1));

    definition.set_type(SCHS);
    command.set_operation(INSERT);
    CommandContext context_insert1(command, *default_logger());
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_insert1, definition, true))
    << "Operation unsupported by device must fail";
    controller_factory.DeleteAllControllers();
    definition.set_type(SCRM);

    const auto device2 = make_shared<MockScsiHd>(LUN, false);
    device2->SetRemovable(true);
    device2->SetProtectable(true);
    device2->SetReady(true);
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device2));

    EXPECT_FALSE(executor->ProcessDeviceCmd(context_attach, definition, true)) << "ID and LUN already exist";

    command.set_operation(START);
    CommandContext context_start(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_start, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_start, definition, false));

    command.set_operation(PROTECT);
    CommandContext context_protect(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_protect, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_protect, definition, false));

    command.set_operation(UNPROTECT);
    CommandContext context_unprotect(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_unprotect, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_unprotect, definition, false));

    command.set_operation(STOP);
    CommandContext context_stop(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_stop, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_stop, definition, false));

    command.set_operation(EJECT);
    CommandContext context_eject(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_eject, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_eject, definition, false));

    command.set_operation(static_cast<PbOperation>(numeric_limits<int32_t>::max()));
    CommandContext context_invalid_command(command, *default_logger());
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_invalid_command, definition, true));
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_invalid_command, definition, false));

    command.set_operation(INSERT);
    SetParam(definition, "file", "filename");
    CommandContext context_insert2(command, *default_logger());
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_insert2, definition, true)) << "Non-existing file";
    EXPECT_FALSE(executor->ProcessDeviceCmd(context_insert2, definition, false)) << "Non-existing file";

    command.set_operation(DETACH);
    CommandContext context_detach(command, *default_logger());
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_detach, definition, true));
    EXPECT_TRUE(executor->ProcessDeviceCmd(context_detach, definition, false));
}

TEST(CommandExecutorTest, ProcessCmd)
{
    const auto bus = make_shared<MockBus>();
    MockAbstractController controller(0);
    ControllerFactory controller_factory;
    const auto executor = make_shared<MockCommandExecutor>(*bus, controller_factory);

    PbCommand command_detach_all;
    command_detach_all.set_operation(DETACH_ALL);
    CommandContext context_detach_all(command_detach_all, *default_logger());
    EXPECT_TRUE(executor->ProcessCmd(context_detach_all));

    PbCommand command_reserve_ids1;
    command_reserve_ids1.set_operation(RESERVE_IDS);
    SetParam(command_reserve_ids1, "ids", "2,3");
    CommandContext context_reserve_ids1(command_reserve_ids1, *default_logger());
    EXPECT_TRUE(executor->ProcessCmd(context_reserve_ids1));
    const unordered_set<int> ids = executor->GetReservedIds();
    EXPECT_EQ(2U, ids.size());
    EXPECT_TRUE(ids.contains(2));
    EXPECT_TRUE(ids.contains(3));

    PbCommand command_reserve_ids2;
    command_reserve_ids2.set_operation(RESERVE_IDS);
    CommandContext context_reserve_ids2(command_reserve_ids2, *default_logger());
    EXPECT_TRUE(executor->ProcessCmd(context_reserve_ids2));
    EXPECT_TRUE(executor->GetReservedIds().empty());

    PbCommand command_reserve_ids3;
    command_reserve_ids3.set_operation(RESERVE_IDS);
    SetParam(command_reserve_ids3, "ids", "-1");
    CommandContext context_reserve_ids3(command_reserve_ids3, *default_logger());
    EXPECT_FALSE(executor->ProcessCmd(context_reserve_ids3));
    EXPECT_TRUE(executor->GetReservedIds().empty());

    PbCommand command_no_operation;
    command_no_operation.set_operation(NO_OPERATION);
    CommandContext context_no_operation(command_no_operation, *default_logger());
    EXPECT_TRUE(executor->ProcessCmd(context_no_operation));

    PbCommand command_attach1;
    command_attach1.set_operation(ATTACH);
    auto *device1 = command_attach1.add_devices();
    device1->set_type(SCHS);
    device1->set_id(-1);
    CommandContext context_attach1(command_attach1, *default_logger());
    EXPECT_FALSE(executor->ProcessCmd(context_attach1)) << "Invalid device ID";

    PbCommand command_attach2;
    command_attach2.set_operation(ATTACH);
    auto *device2 = command_attach2.add_devices();
    device2->set_type(SCHS);
    device2->set_id(0);
    device2->set_unit(1);
    CommandContext context_attach2(command_attach2, *default_logger());
    EXPECT_FALSE(executor->ProcessCmd(context_attach2)) << "LUN 0 is missing";
}

TEST(CommandExecutorTest, Attach)
{
    const int ID = 3;
    const int LUN = 0;

    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbDeviceDefinition definition;
    PbCommand command;
    CommandContext context(command, *default_logger());

    definition.set_unit(32);
    EXPECT_FALSE(executor->Attach(context, definition, false));

    const auto device = DeviceFactory::GetInstance().CreateDevice(SCHD, LUN, "");
    definition.set_id(ID);
    definition.set_unit(LUN);

    executor->SetReservedIds("3");
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Reserved ID not rejected";

    executor->SetReservedIds("");
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Unknown device type not rejected";

    definition.set_type(SCHS);
    EXPECT_TRUE(executor->Attach(context, definition, false));
    controller_factory.DeleteAllControllers();

    definition.set_type(SCHD);
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Drive without sectors not rejected";

    definition.set_revision("invalid revision");
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Drive with invalid revision not rejected";
    definition.set_revision("1234");

    definition.set_block_size(1);
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Drive with invalid sector size not rejected";

    definition.set_block_size(512);
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Drive without image file not rejected";

    SetParam(definition, "file", "/non_existing_file");
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Drive with non-existing image file not rejected";

    path filename = CreateTempFile(1);
    SetParam(definition, "file", filename.string());
    EXPECT_FALSE(executor->Attach(context, definition, false)) << "Too small image file not rejected";

    filename = CreateTempFile(512);
    SetParam(definition, "file", filename.string());
    bool result = executor->Attach(context, definition, false);
    EXPECT_TRUE(result);
    controller_factory.DeleteAllControllers();

    filename = CreateTempFile(513);
    SetParam(definition, "file", filename.string());
    result = executor->Attach(context, definition, false);
    EXPECT_TRUE(result);

    definition.set_type(SCCD);
    definition.set_unit(LUN + 1);
    filename = CreateTempFile(2048);
    SetParam(definition, "file", filename.string());
    result = executor->Attach(context, definition, false);
    EXPECT_TRUE(result);

    definition.set_type(SCMO);
    definition.set_unit(LUN + 2);
    SetParam(definition, "read_only", "true");
    filename = CreateTempFile(4096);
    SetParam(definition, "file", filename.string());
    result = executor->Attach(context, definition, false);
    EXPECT_TRUE(result);

    controller_factory.DeleteAllControllers();
}

TEST(CommandExecutorTest, Insert)
{
    const auto bus = make_shared<MockBus>();
    const auto [controller, device] = CreateDevice(SCHD);
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbDeviceDefinition definition;
    PbCommand command;
    CommandContext context(command, *default_logger());

    device->SetRemoved(false);
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Medium is not removed";

    device->SetRemoved(true);
    definition.set_vendor("v");
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Product data must not be set";
    definition.set_vendor("");

    definition.set_product("p");
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Product data must not be set";
    definition.set_product("");

    definition.set_revision("r");
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Product data must not be set";
    definition.set_revision("");

    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Filename is missing";

    SetParam(definition, "file", "filename");
    EXPECT_TRUE(executor->Insert(context, definition, device, true)) << "Dry-run must not fail";
    EXPECT_FALSE(executor->Insert(context, definition, device, false));

    definition.set_block_size(1);
    EXPECT_FALSE(executor->Insert(context, definition, device, false));

    definition.set_block_size(0);
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Image file validation must fail";

    SetParam(definition, "file", "/non_existing_file");
    EXPECT_FALSE(executor->Insert(context, definition, device, false));

    path filename = CreateTempFile(1);
    SetParam(definition, "file", filename.string());
    EXPECT_FALSE(executor->Insert(context, definition, device, false)) << "Too small image file not rejected";

    filename = CreateTempFile(512);
    SetParam(definition, "file", filename.string());
    static_pointer_cast<Disk>(device)->SetCachingMode(PbCachingMode::PISCSI);
    const bool result = executor->Insert(context, definition, device, false);
    EXPECT_TRUE(result);
}

TEST(CommandExecutorTest, Detach)
{
    const int ID = 3;
    const int LUN1 = 0;
    const int LUN2 = 1;

    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    CommandContext context(command, *default_logger());

    const auto device1 = DeviceFactory::GetInstance().CreateDevice(SCHS, LUN1, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device1));
    const auto device2 = DeviceFactory::GetInstance().CreateDevice(SCHS, LUN2, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device2));

    const auto d1 = controller_factory.GetDeviceForIdAndLun(ID, LUN1);
    EXPECT_FALSE(executor->Detach(context, *d1, false)) << "LUNs > 0 have to be detached first";
    const auto d2 = controller_factory.GetDeviceForIdAndLun(ID, LUN2);
    EXPECT_TRUE(executor->Detach(context, *d2, false));
    EXPECT_TRUE(executor->Detach(context, *d1, false));
    EXPECT_TRUE(controller_factory.GetAllDevices().empty());
}

TEST(CommandExecutorTest, DetachAll)
{
    const int ID = 4;

    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());

    const auto device = DeviceFactory::GetInstance().CreateDevice(SCHS, 0, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device));
    EXPECT_NE(nullptr, device->GetController());
    EXPECT_FALSE(controller_factory.GetAllDevices().empty());

    executor->DetachAll();
    EXPECT_TRUE(controller_factory.GetAllDevices().empty());
}

TEST(CommandExecutorTest, SetReservedIds)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());

    string error = executor->SetReservedIds("xyz");
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(executor->GetReservedIds().empty());

    error = executor->SetReservedIds("8");
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(executor->GetReservedIds().empty());

    error = executor->SetReservedIds("-1");
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(executor->GetReservedIds().empty());

    error = executor->SetReservedIds("");
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(executor->GetReservedIds().empty());

    error = executor->SetReservedIds("7,1,2,3,5");
    EXPECT_TRUE(error.empty());
    const unordered_set<int> reserved_ids = executor->GetReservedIds();
    EXPECT_EQ(5U, reserved_ids.size());
    EXPECT_TRUE(reserved_ids.contains(1));
    EXPECT_TRUE(reserved_ids.contains(2));
    EXPECT_TRUE(reserved_ids.contains(3));
    EXPECT_TRUE(reserved_ids.contains(5));
    EXPECT_TRUE(reserved_ids.contains(7));

    const auto device = DeviceFactory::GetInstance().CreateDevice(SCHS, 0, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, 5, device));
    error = executor->SetReservedIds("5");
    EXPECT_FALSE(error.empty());
}

TEST(CommandExecutorTest, ValidateImageFile)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    CommandContext context(command, *default_logger());

    const auto device = static_pointer_cast<StorageDevice>(DeviceFactory::GetInstance().CreateDevice(SCHD, 0, "test"));
    EXPECT_TRUE(executor->ValidateImageFile(context, *device, ""));

    EXPECT_FALSE(executor->ValidateImageFile(context, *device, "/non_existing_file"));
}

TEST(CommandExecutorTest, PrintCommand)
{
    PbDeviceDefinition definition;
    PbCommand command;

    string s = CommandExecutor::PrintCommand(command, definition);
    EXPECT_NE(s.find("operation="), string::npos);
    EXPECT_EQ(s.find("key1=value1"), string::npos);
    EXPECT_EQ(s.find("key2=value2"), string::npos);

    SetParam(command, "key1", "value1");
    s = CommandExecutor::PrintCommand(command, definition);
    EXPECT_NE(s.find("operation="), string::npos);
    EXPECT_NE(s.find("key1=value1"), string::npos);

    SetParam(command, "key2", "value2");
    s = CommandExecutor::PrintCommand(command, definition);
    EXPECT_NE(s.find("operation="), string::npos);
    EXPECT_NE(s.find("key1=value1"), string::npos);
    EXPECT_NE(s.find("key2=value2"), string::npos);
}

TEST(CommandExecutorTest, EnsureLun0)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    CommandContext context(command, *default_logger());

    auto *device1 = command.add_devices();
    device1->set_unit(0);
    EXPECT_TRUE(executor->EnsureLun0(context, command));

    device1->set_unit(1);
    EXPECT_FALSE(executor->EnsureLun0(context, command));

    const auto device2 = DeviceFactory::GetInstance().CreateDevice(SCHS, 0, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, 0, device2));
    EXPECT_TRUE(executor->EnsureLun0(context, command));
}

TEST(CommandExecutorTest, CreateDevice)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbDeviceDefinition device;
    PbCommand command;
    CommandContext context(command, *default_logger());

    device.set_type(UNDEFINED);
    EXPECT_EQ(nullptr, executor->CreateDevice(context, device));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    device.set_type(SCBR);
    EXPECT_EQ(nullptr, executor->CreateDevice(context, device));
#pragma GCC diagnostic pop
    device.set_type(SCHS);
    EXPECT_NE(nullptr, executor->CreateDevice(context, device));
    device.set_type(UNDEFINED);
    SetParam(device, "file", "services");
    EXPECT_NE(nullptr, executor->CreateDevice(context, device));
}

TEST(CommandExecutorTest, SetBlockSize)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    CommandContext context(command, *default_logger());

    set<uint32_t> sizes;

    sizes.insert(512);
    auto hd = make_shared<MockScsiHd>(sizes);
    EXPECT_TRUE(executor->SetBlockSize(context, hd, 0));
    EXPECT_FALSE(executor->SetBlockSize(context, hd, 1));
    EXPECT_TRUE(executor->SetBlockSize(context, hd, 512));

    sizes.insert(1024);
    hd = make_shared<MockScsiHd>(sizes);
    EXPECT_TRUE(executor->SetBlockSize(context, hd, 512));
}

TEST(CommandExecutorTest, ValidateOperation)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command_attach;
    command_attach.set_operation(ATTACH);
    CommandContext context_attach(command_attach, *default_logger());
    PbCommand command_detach;
    command_detach.set_operation(DETACH);
    CommandContext context_detach(command_detach, *default_logger());
    PbCommand command_start;
    command_start.set_operation(START);
    CommandContext context_start(command_start, *default_logger());
    PbCommand command_stop;
    command_stop.set_operation(STOP);
    CommandContext context_stop(command_stop, *default_logger());
    PbCommand command_insert;
    command_insert.set_operation(INSERT);
    CommandContext context_insert(command_insert, *default_logger());
    PbCommand command_eject;
    command_eject.set_operation(EJECT);
    CommandContext context_eject(command_eject, *default_logger());
    PbCommand command_protect;
    command_protect.set_operation(PROTECT);
    CommandContext context_protect(command_protect, *default_logger());
    PbCommand command_unprotect;
    command_unprotect.set_operation(UNPROTECT);
    CommandContext context_unprotect(command_unprotect, *default_logger());

    const auto device = make_shared<MockPrimaryDevice>(0);

    EXPECT_TRUE(executor->ValidateOperation(context_attach, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_detach, *device));

    EXPECT_FALSE(executor->ValidateOperation(context_start, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_stop, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_insert, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_eject, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_protect, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_unprotect, *device));

    device->SetStoppable(true);
    EXPECT_TRUE(executor->ValidateOperation(context_start, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_stop, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_insert, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_eject, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_protect, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_unprotect, *device));

    device->SetRemovable(true);
    EXPECT_TRUE(executor->ValidateOperation(context_start, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_stop, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_insert, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_eject, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_protect, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_unprotect, *device));

    device->SetProtectable(true);
    EXPECT_TRUE(executor->ValidateOperation(context_start, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_stop, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_insert, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_eject, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_protect, *device));
    EXPECT_FALSE(executor->ValidateOperation(context_unprotect, *device));

    device->SetReady(true);
    EXPECT_TRUE(executor->ValidateOperation(context_start, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_stop, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_insert, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_eject, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_protect, *device));
    EXPECT_TRUE(executor->ValidateOperation(context_unprotect, *device));
}

TEST(CommandExecutorTest, ValidateDevice)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    command.set_operation(ATTACH);
    CommandContext context_attach(command, *default_logger());
    PbDeviceDefinition device;

    device.set_type(SCHD);
    device.set_id(-1);
    EXPECT_FALSE(executor->ValidateDevice(context_attach, device));
    device.set_id(8);
    EXPECT_FALSE(executor->ValidateDevice(context_attach, device));
    device.set_id(7);
    device.set_unit(-1);
    EXPECT_FALSE(executor->ValidateDevice(context_attach, device));
    device.set_unit(32);
    EXPECT_FALSE(executor->ValidateDevice(context_attach, device));
    device.set_unit(0);
    EXPECT_TRUE(executor->ValidateDevice(context_attach, device));
    device.set_unit(31);
    EXPECT_TRUE(executor->ValidateDevice(context_attach, device));
    device.set_type(SAHD);
    device.set_unit(1);
    EXPECT_TRUE(executor->ValidateDevice(context_attach, device));
    device.set_unit(2);
    EXPECT_FALSE(executor->ValidateDevice(context_attach, device));

    const auto d = DeviceFactory::GetInstance().CreateDevice(SCHS, 0, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, 1, d));
    command.set_operation(DETACH);
    CommandContext context_detach(command, *default_logger());
    device.set_id(1);
    device.set_unit(4);
    EXPECT_FALSE(executor->ValidateDevice(context_detach, device));
    device.set_id(1);
    EXPECT_FALSE(executor->ValidateDevice(context_detach, device));
    device.set_unit(0);
    EXPECT_TRUE(executor->ValidateDevice(context_detach, device));
}

TEST(CommandExecutorTest, SetProductData)
{
    const auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory;
    const auto executor = make_shared<CommandExecutor>(*bus, controller_factory, *default_logger());
    PbCommand command;
    CommandContext context(command, *default_logger());
    PbDeviceDefinition definition;

    const auto device = make_shared<MockPrimaryDevice>(0);

    EXPECT_TRUE(executor->SetProductData(context, definition, *device));

    definition.set_vendor("123456789");
    EXPECT_FALSE(executor->SetProductData(context, definition, *device));
    definition.set_vendor("1");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));
    definition.set_vendor("12345678");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));

    definition.set_product("12345678901234567");
    EXPECT_FALSE(executor->SetProductData(context, definition, *device));
    definition.set_product("1");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));
    definition.set_product("1234567890123456");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));

    definition.set_revision("12345");
    EXPECT_FALSE(executor->SetProductData(context, definition, *device));
    definition.set_revision("1");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));
    definition.set_revision("1234");
    EXPECT_TRUE(executor->SetProductData(context, definition, *device));
}
