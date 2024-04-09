//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/device_factory.h"
#include "controllers/controller_factory.h"

TEST(ControllerFactoryTest, LifeCycle)
{
    const int ID1 = 4;
    const int ID2 = 5;
    const int LUN1 = 0;
    const int LUN2 = 1;

    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    EXPECT_FALSE(controller_factory.AttachToController(bus, ID1, device_factory.CreateDevice(SCHS, -1, "")));

    EXPECT_TRUE(controller_factory.AttachToController(bus, ID1, device_factory.CreateDevice(SCHS, LUN1, "")));
    EXPECT_TRUE(controller_factory.HasController(ID1));
    auto controller = controller_factory.FindController(ID1);
    EXPECT_NE(nullptr, controller);
    EXPECT_EQ(1U, controller->GetLunCount());
    EXPECT_FALSE(controller_factory.HasController(0));
    EXPECT_EQ(nullptr, controller_factory.FindController(0));
    EXPECT_TRUE(controller_factory.HasDeviceForIdAndLun(ID1, LUN1));
    EXPECT_NE(nullptr, controller_factory.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory.HasDeviceForIdAndLun(0, 0));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(0, 0));

    EXPECT_TRUE(controller_factory.AttachToController(bus, ID1, device_factory.CreateDevice(SCHS, LUN2, "")));
    EXPECT_TRUE(controller_factory.HasController(ID1));
    controller = controller_factory.FindController(ID1);
    EXPECT_NE(nullptr, controller_factory.FindController(ID1));
    EXPECT_TRUE(controller_factory.DeleteController(*controller));
    EXPECT_EQ(nullptr, controller_factory.FindController(ID1));

    const auto disk = make_shared<MockDisk>();
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID2, disk));
    EXPECT_CALL(*disk, FlushCache);
    controller_factory.DeleteAllControllers();
    EXPECT_FALSE(controller_factory.HasController(ID1));
    EXPECT_EQ(nullptr, controller_factory.FindController(ID1));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory.HasDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory.HasController(ID2));
    EXPECT_EQ(nullptr, controller_factory.FindController(ID2));
    EXPECT_EQ(nullptr, controller_factory.GetDeviceForIdAndLun(ID2, LUN1));
    EXPECT_FALSE(controller_factory.HasDeviceForIdAndLun(ID2, LUN1));
    EXPECT_FALSE(controller_factory.HasController(ID1));
}

TEST(ControllerFactoryTest, AttachToController)
{
    const int ID = 4;
    const int LUN1 = 3;
    const int LUN2 = 0;

    MockBus bus;
    ControllerFactory controller_factory;
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    const auto device = device_factory.CreateDevice(SCHS, LUN1, "");
    EXPECT_FALSE(controller_factory.AttachToController(bus, ID, device)) << "LUN 0 is missing";

    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device_factory.CreateDevice(SCLP, LUN2, "")));
    EXPECT_TRUE(controller_factory.AttachToController(bus, ID, device));
    EXPECT_FALSE(controller_factory.AttachToController(bus, ID, device));
}

TEST(ControllerFactory, ProcessOnController)
{
    ControllerFactory controller_factory;

    EXPECT_EQ(AbstractController::shutdown_mode::none, controller_factory.ProcessOnController(0));
}

TEST(ControllerFactory, GetScsiLunMax)
{
    EXPECT_EQ(32, ControllerFactory::GetScsiLunMax());
}

TEST(ControllerFactory, GetSasiLunMax)
{
    EXPECT_EQ(2, ControllerFactory::GetSasiLunMax());
}
