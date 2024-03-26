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

    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory_scsi(false);
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    auto device = device_factory.CreateDevice(SCHS, -1, "");
    EXPECT_FALSE(controller_factory_scsi.AttachToController(*bus, ID1, device));

    device = device_factory.CreateDevice(SCHS, LUN1, "");
    EXPECT_TRUE(controller_factory_scsi.AttachToController(*bus, ID1, device));
    EXPECT_TRUE(controller_factory_scsi.HasController(ID1));
    auto controller = controller_factory_scsi.FindController(ID1);
    EXPECT_NE(nullptr, controller);
    EXPECT_EQ(1U, controller->GetLunCount());
    EXPECT_FALSE(controller_factory_scsi.HasController(0));
    EXPECT_EQ(nullptr, controller_factory_scsi.FindController(0));
    EXPECT_TRUE(controller_factory_scsi.HasDeviceForIdAndLun(ID1, LUN1));
    EXPECT_NE(nullptr, controller_factory_scsi.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory_scsi.HasDeviceForIdAndLun(0, 0));
    EXPECT_EQ(nullptr, controller_factory_scsi.GetDeviceForIdAndLun(0, 0));

    device = device_factory.CreateDevice(SCHS, LUN2, "");
    EXPECT_TRUE(controller_factory_scsi.AttachToController(*bus, ID1, device));
    EXPECT_TRUE(controller_factory_scsi.HasController(ID1));
    controller = controller_factory_scsi.FindController(ID1);
    EXPECT_NE(nullptr, controller_factory_scsi.FindController(ID1));
    EXPECT_TRUE(controller_factory_scsi.DeleteController(*controller));
    EXPECT_EQ(nullptr, controller_factory_scsi.FindController(ID1));

    auto disk = make_shared<MockDisk>();
    EXPECT_TRUE(controller_factory_scsi.AttachToController(*bus, ID2, disk));
    EXPECT_CALL(*disk, FlushCache);
    controller_factory_scsi.DeleteAllControllers();
    EXPECT_FALSE(controller_factory_scsi.HasController(ID1));
    EXPECT_EQ(nullptr, controller_factory_scsi.FindController(ID1));
    EXPECT_EQ(nullptr, controller_factory_scsi.GetDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory_scsi.HasDeviceForIdAndLun(ID1, LUN1));
    EXPECT_FALSE(controller_factory_scsi.HasController(ID2));
    EXPECT_EQ(nullptr, controller_factory_scsi.FindController(ID2));
    EXPECT_EQ(nullptr, controller_factory_scsi.GetDeviceForIdAndLun(ID2, LUN1));
    EXPECT_FALSE(controller_factory_scsi.HasDeviceForIdAndLun(ID2, LUN1));

    device = device_factory.CreateDevice(SAHD, LUN1, "");
    EXPECT_FALSE(controller_factory_scsi.AttachToController(*bus, ID1, device));
    EXPECT_FALSE(controller_factory_scsi.HasController(ID1));

    ControllerFactory controller_factory_sasi(true);
    EXPECT_TRUE(controller_factory_sasi.AttachToController(*bus, ID1, device));
    EXPECT_TRUE(controller_factory_sasi.HasController(ID1));

    device = device_factory.CreateDevice(SAHD, 7, "");
    EXPECT_FALSE(controller_factory_sasi.AttachToController(*bus, ID2, device));
    EXPECT_FALSE(controller_factory_sasi.HasController(ID2));

    device = device_factory.CreateDevice(SCHD, LUN2, "");
    EXPECT_FALSE(controller_factory_sasi.AttachToController(*bus, ID2, device));
    EXPECT_FALSE(controller_factory_sasi.HasController(ID2));
}

TEST(ControllerFactoryTest, AttachToController)
{
    const int ID = 4;
    const int LUN1 = 3;
    const int LUN2 = 0;

    auto bus = make_shared<MockBus>();
    ControllerFactory controller_factory(false);
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    auto device1 = device_factory.CreateDevice(SCHS, LUN1, "");
    EXPECT_FALSE(controller_factory.AttachToController(*bus, ID, device1)) << "LUN 0 is missing";

    auto device2 = device_factory.CreateDevice(SCLP, LUN2, "");
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device2));
    EXPECT_TRUE(controller_factory.AttachToController(*bus, ID, device1));
    EXPECT_FALSE(controller_factory.AttachToController(*bus, ID, device1));
}

TEST(ControllerFactory, ProcessOnController)
{
    ControllerFactory controller_factory(false);

    EXPECT_EQ(AbstractController::shutdown_mode::none, controller_factory.ProcessOnController(0));
}

TEST(ControllerFactory, GetLunMax)
{
    ControllerFactory controller_factory1(false);
    EXPECT_EQ(32, ControllerFactory::GetLunMax());

    ControllerFactory controller_factory2(true);
    EXPECT_EQ(2, ControllerFactory::GetLunMax());
}

TEST(ControllerFactory, GetScsiLunMax)
{
    EXPECT_EQ(32, ControllerFactory::GetScsiLunMax());
}

TEST(ControllerFactory, GetSasiLunMax)
{
    EXPECT_EQ(2, ControllerFactory::GetSasiLunMax());
}
