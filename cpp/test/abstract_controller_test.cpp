//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

using namespace scsi_defs;

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
    auto controller = make_shared<MockAbstractController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->AddDevice(device);

    controller->SetPhase(phase_t::status);
    EXPECT_EQ(phase_t::status, controller->GetPhase());
    EXPECT_CALL(*bus, Reset());
    controller->Reset();
    EXPECT_TRUE(controller->IsBusFree());
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_EQ(0, controller->GetCurrentLength());
}

TEST(AbstractControllerTest, Status)
{
    MockAbstractController controller;

    controller.SetStatus(status::reservation_conflict);
    EXPECT_EQ(status::reservation_conflict, controller.GetStatus());
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
    const int ID = 1;
    const int LUN = 4;

    auto controller = make_shared<NiceMock<MockAbstractController>>(ID);

    auto device1 = make_shared<MockPrimaryDevice>(LUN);
    auto device2 = make_shared<MockPrimaryDevice>(32);
    auto device3 = make_shared<MockPrimaryDevice>(-1);

    EXPECT_EQ(0U, controller->GetLunCount());
    EXPECT_EQ(ID, controller->GetTargetId());
    EXPECT_TRUE(controller->AddDevice(device1));
    EXPECT_FALSE(controller->AddDevice(device2));
    EXPECT_FALSE(controller->AddDevice(device3));
    EXPECT_TRUE(controller->GetLunCount() > 0);
    EXPECT_NE(nullptr, controller->GetDeviceForLun(LUN));
    EXPECT_EQ(nullptr, controller->GetDeviceForLun(0));
    EXPECT_TRUE(controller->RemoveDevice(*device1));
    EXPECT_EQ(0L, controller->GetLunCount());
    EXPECT_FALSE(controller->RemoveDevice(*device1));
}

TEST(AbstractControllerTest, GetOpcode)
{
    MockAbstractController controller;

    controller.SetCdbByte(0, static_cast<int>(scsi_command::cmd_inquiry));
    EXPECT_EQ(scsi_command::cmd_inquiry, controller.GetOpcode());
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
    auto bus = make_shared<MockBus>();
    auto controller = make_shared<MockAbstractController>(bus, 1);

    EXPECT_CALL(*controller, Process());
    controller->ProcessOnController(0x02);
    EXPECT_CALL(*controller, Process());
    controller->ProcessOnController(0x06);
}
