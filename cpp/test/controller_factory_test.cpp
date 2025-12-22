//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/device_factory.h"
#include "shared/s2p_defs.h"
#include "controllers/controller_factory.h"

TEST(ControllerFactoryTest, LifeCycle)
{
    const int ID1 = 4;
    const int ID2 = 5;
    const int LUN1 = 0;
    const int LUN2 = 1;

    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::GetInstance();

    auto device = device_factory.CreateDevice(SCHS, -1, "");
    EXPECT_FALSE(controller_factory.AttachToController(bus, ID1, device));

    device = device_factory.CreateDevice(SCHS, LUN1, "");
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID1, device));
    EXPECT_TRUE(controller_factory.HasController(ID1));
    EXPECT_NE(nullptr, device->GetController());
    EXPECT_EQ(1U, device->GetController()->GetLunCount());
    EXPECT_FALSE(controller_factory.HasController(0));
    EXPECT_NE(nullptr, controller_factory.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(0, 0));

    device = device_factory.CreateDevice(SCHS, LUN2, "");
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID1, device));
    EXPECT_TRUE(controller_factory.HasController(ID1));
    EXPECT_NE(nullptr, device->GetController());
    EXPECT_EQ(2U, device->GetController()->GetLunCount());
    EXPECT_TRUE(controller_factory.DeleteController(*device->GetController()));

    const auto disk = make_shared<MockDisk>();
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID2, disk));
    EXPECT_CALL(*disk, FlushCache);
    controller_factory.DeleteAllControllers();
    EXPECT_FALSE(controller_factory.HasController(ID1));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory.HasController(ID2));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(ID2, LUN1));
    EXPECT_FALSE(controller_factory.HasController(ID1));
}

TEST(ControllerFactoryTest, AttachToController)
{
    const int ID = 4;
    const int LUN1 = 3;
    const int LUN2 = 0;

    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::GetInstance();

    const auto device = device_factory.CreateDevice(SCHS, LUN1, "");
    EXPECT_FALSE(controller_factory.AttachToController(bus, ID, device)) << "LUN 0 is missing";

    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device_factory.CreateDevice(SCLP, LUN2, "")));
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device));
    EXPECT_FALSE(controller_factory.AttachToController(bus, ID, device));
}

TEST(ControllerFactoryTest, SetScriptFile)
{
    ControllerFactory controller_factory;

    EXPECT_FALSE(controller_factory.SetScriptFile(""));
    const string &filename = CreateTempFile();
    EXPECT_TRUE(controller_factory.SetScriptFile(filename));
    ifstream file(filename);
    EXPECT_TRUE(file.good());
}

TEST(ControllerFactoryTest, ProcessOnController)
{
    const int VALID_ID = 0;
    const int INVALID_ID = 1;

    NiceMock<MockBus> bus;
    ControllerFactory controller_factory;

    EXPECT_EQ(ShutdownMode::NONE, controller_factory.ProcessOnController(VALID_ID));

    const auto device = make_shared<MockPrimaryDevice>(0);
    EXPECT_TRUE(controller_factory.AttachToController(bus, VALID_ID, device));

    EXPECT_EQ(ShutdownMode::NONE, controller_factory.ProcessOnController(VALID_ID));

    EXPECT_EQ(ShutdownMode::NONE, controller_factory.ProcessOnController(INVALID_ID));
}

TEST(ControllerFactoryTest, SetLogLevel)
{
    const int ID = 4;
    const int LUN1 = 0;
    const int LUN2 = 3;

    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::GetInstance();

    const auto device1 = device_factory.CreateDevice(SCHS, LUN1, "");
    controller_factory.AttachToController(bus, ID, device1);
    const auto device2 = device_factory.CreateDevice(SCHS, LUN2, "");
    controller_factory.AttachToController(bus, ID, device2);

    controller_factory.SetLogLevel(ID, LUN1, level::level_enum::off);
    controller_factory.SetLogLevel(ID, LUN2, level::level_enum::off);
    EXPECT_EQ(level::level_enum::off, device1->GetLogger().level());
    EXPECT_EQ(level::level_enum::off, device2->GetLogger().level());

    controller_factory.SetLogLevel(ID, LUN1, level::level_enum::critical);
    EXPECT_EQ(level::level_enum::critical, device1->GetLogger().level());
    EXPECT_EQ(level::level_enum::off, device2->GetLogger().level());

    controller_factory.SetLogLevel(ID, LUN2, level::level_enum::err);
    EXPECT_EQ(level::level_enum::off, device1->GetLogger().level());
    EXPECT_EQ(level::level_enum::err, device2->GetLogger().level());
}

TEST(ControllerFactoryTest, SetFormatLimit)
{
    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::GetInstance();
    const array<const uint8_t, 2> bytes = { 0x01, 0x02 };

    const auto device1 = device_factory.CreateDevice(SCHS, 0, "");
    controller_factory.AttachToController(bus, 0, device1);
    EXPECT_EQ("01:02", device1->GetController()->FormatBytes(bytes, bytes.size()).substr(10, 5));

    controller_factory.SetFormatLimit(1);
    const auto device2 = device_factory.CreateDevice(SCHS, 0, "");
    controller_factory.AttachToController(bus, 1, device2);
    EXPECT_EQ("01   ", device2->GetController()->FormatBytes(bytes, bytes.size()).substr(10, 5));
}
