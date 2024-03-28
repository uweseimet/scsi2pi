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

TEST(ControllerTest, Reset)
{
    const int TARGET_ID = 5;
    const int INITIATOR_ID = 7;

    NiceMock<MockBus> bus;
    auto controller = make_shared<Controller>(bus, TARGET_ID, 32);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->Init();
    controller->AddDevice(device);

    controller->ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller->GetInitiatorId());
    controller->Reset();
    EXPECT_EQ(-1, controller->GetInitiatorId());
}

TEST(ControllerTest, GetInitiatorId)
{
    const int TARGET_ID = 0;
    const int INITIATOR_ID = 2;

    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, TARGET_ID);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.Init();
    controller.AddDevice(device);

    controller.ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller.GetInitiatorId());
}

TEST(ControllerTest, BusFree)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);

    controller.SetPhase(phase_t::busfree);
    controller.BusFree();
    EXPECT_EQ(phase_t::busfree, controller.GetPhase());

    controller.SetStatus(status::check_condition);
    controller.SetPhase(phase_t::reserved);
    controller.BusFree();
    EXPECT_EQ(phase_t::busfree, controller.GetPhase());
    EXPECT_EQ(status::good, controller.GetStatus());

    controller.ScheduleShutdown(AbstractController::shutdown_mode::none);
    controller.SetPhase(phase_t::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(AbstractController::shutdown_mode::stop_pi);
    controller.SetPhase(phase_t::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(AbstractController::shutdown_mode::restart_pi);
    controller.SetPhase(phase_t::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(AbstractController::shutdown_mode::stop_s2p);
    controller.SetPhase(phase_t::reserved);
    controller.BusFree();
}

TEST(ControllerTest, Selection)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    auto controller = make_shared<MockController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->AddDevice(device);

    controller->SetPhase(phase_t::selection);
    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());

    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());

    ON_CALL(*bus, GetDAT).WillByDefault(Return(1));
    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());
}

TEST(ControllerTest, Command)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.AddDevice(device);

    controller.SetPhase(phase_t::command);
    EXPECT_CALL(controller, Status).Times(2);
    controller.Command();
    EXPECT_EQ(phase_t::command, controller.GetPhase());

    controller.SetPhase(phase_t::reserved);
    controller.Command();
    EXPECT_EQ(phase_t::command, controller.GetPhase());

    controller.SetPhase(phase_t::reserved);
    controller.Command();
    EXPECT_EQ(phase_t::command, controller.GetPhase());
}

TEST(ControllerTest, MsgIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.MsgIn();
    EXPECT_EQ(phase_t::msgin, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(ControllerTest, MsgOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.MsgOut();
    EXPECT_EQ(phase_t::msgout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(1, controller.GetCurrentLength());
}

TEST(ControllerTest, DataIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataIn();
    EXPECT_EQ(phase_t::reserved, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataIn();
    EXPECT_EQ(phase_t::datain, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ControllerTest, DataOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataOut();
    EXPECT_EQ(phase_t::reserved, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataOut();
    EXPECT_EQ(phase_t::dataout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ControllerTest, RequestSense)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);
    EXPECT_TRUE(device->Init( { }));

    controller.AddDevice(device);

    // ALLOCATION LENGTH
    controller.SetCdbByte(4, 255);
    // Non-existing LUN
    controller.SetCdbByte(1, 0x20);

    device->SetReady(true);
    EXPECT_CALL(controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::cmd_request_sense));
    EXPECT_EQ(status::good, controller.GetStatus()) << "Wrong CHECK CONDITION for non-existing LUN";
}
