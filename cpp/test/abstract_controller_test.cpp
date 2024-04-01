//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

TEST(AbstractControllerTest, ShutdownMode)
{
    MockAbstractController controller;

    EXPECT_CALL(controller, Process);
    EXPECT_EQ(AbstractController::shutdown_mode::none, controller.ProcessOnController(0));
    controller.ScheduleShutdown(AbstractController::shutdown_mode::stop_s2p);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(AbstractController::shutdown_mode::stop_s2p, controller.ProcessOnController(0));
    controller.ScheduleShutdown(AbstractController::shutdown_mode::stop_pi);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(AbstractController::shutdown_mode::stop_pi, controller.ProcessOnController(0));
    controller.ScheduleShutdown(AbstractController::shutdown_mode::restart_pi);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(AbstractController::shutdown_mode::restart_pi, controller.ProcessOnController(0));
}

TEST(AbstractControllerTest, SetCurrentLength)
{
    MockAbstractController controller;

    EXPECT_EQ(4096U, controller.GetBuffer().size());
    controller.SetCurrentLength(1);
    EXPECT_LE(1U, controller.GetBuffer().size());
    controller.SetCurrentLength(10000);
    EXPECT_LE(10000U, controller.GetBuffer().size());
}

TEST(AbstractControllerTest, Reset)
{
    auto bus = make_shared<MockBus>();
    MockAbstractController controller(bus, 0);

    controller.AddDevice(make_shared<MockPrimaryDevice>(0));

    controller.SetPhase(bus_phase::status);
    EXPECT_EQ(bus_phase::status, controller.GetPhase());
    EXPECT_CALL(*bus, Reset());
    controller.Reset();
    EXPECT_TRUE(controller.IsBusFree());
    EXPECT_EQ(status_code::good, controller.GetStatus());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(AbstractControllerTest, Status)
{
    MockAbstractController controller;

    controller.SetStatus(status_code::reservation_conflict);
    EXPECT_EQ(status_code::reservation_conflict, controller.GetStatus());
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
    const int ID = 1;
    const int LUN = 4;

    MockAbstractController controller(ID);

    auto device = make_shared<MockPrimaryDevice>(LUN);

    EXPECT_EQ(0U, controller.GetLunCount());
    EXPECT_EQ(ID, controller.GetTargetId());
    EXPECT_TRUE(controller.AddDevice(device));
    EXPECT_FALSE(controller.AddDevice(make_shared<MockPrimaryDevice>(32)));
    EXPECT_FALSE(controller.AddDevice(make_shared<MockPrimaryDevice>(-1)));
    EXPECT_TRUE(controller.GetLunCount() > 0);
    EXPECT_NE(nullptr, controller.GetDeviceForLun(LUN));
    EXPECT_EQ(nullptr, controller.GetDeviceForLun(0));
    EXPECT_TRUE(controller.RemoveDevice(*device));
    EXPECT_EQ(0U, controller.GetLunCount());
    EXPECT_FALSE(controller.RemoveDevice(*device));
}

TEST(AbstractControllerTest, TransferSize)
{
    MockAbstractController controller;

    controller.SetTransferSize(3, 1);
    EXPECT_EQ(1, controller.GetChunkSize());
    EXPECT_TRUE(controller.UpdateTransferSize());
    EXPECT_TRUE(controller.UpdateTransferSize());
    EXPECT_FALSE(controller.UpdateTransferSize());
}

TEST(AbstractControllerTest, UpdateOffsetAndLength)
{
    MockAbstractController controller;

    controller.UpdateOffsetAndLength();
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(AbstractControllerTest, Offset)
{
    MockAbstractController controller;

    controller.ResetOffset();
    EXPECT_EQ(0, controller.GetOffset());

    controller.UpdateOffsetAndLength();
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(AbstractControllerTest, ProcessOnController)
{
    MockAbstractController controller(make_shared<MockBus>(), 1);

    EXPECT_CALL(controller, Process());
    controller.ProcessOnController(0x02);
    EXPECT_CALL(controller, Process());
    controller.ProcessOnController(0x06);
}
