//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

using namespace scsi_defs;

TEST(ScsiControllerTest, Reset)
{
    const int ID = 5;

    NiceMock<MockBus> bus;
    auto controller = make_shared<Controller>(bus, ID, 32);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->Init();
    controller->AddDevice(device);

    controller->Process(ID);
    EXPECT_EQ(ID, controller->GetInitiatorId());
    controller->Reset();
    EXPECT_EQ(-1, controller->GetInitiatorId());
}

TEST(ScsiControllerTest, GetInitiatorId)
{
    const int ID = 2;

    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.Init();
    controller.AddDevice(device);

    controller.Process(ID);
    EXPECT_EQ(ID, controller.GetInitiatorId());
    controller.Process(1234);
    EXPECT_EQ(1234, controller.GetInitiatorId());
}

TEST(ScsiControllerTest, BusFree)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

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

TEST(ScsiControllerTest, Selection)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    auto controller = make_shared<MockScsiController>(bus, 0);
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

TEST(ScsiControllerTest, Command)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);
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

TEST(ScsiControllerTest, MsgIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.MsgIn();
    EXPECT_EQ(phase_t::msgin, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(ScsiControllerTest, MsgOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    controller.MsgOut();
    EXPECT_EQ(phase_t::msgout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(1, controller.GetCurrentLength());
}

TEST(ScsiControllerTest, DataIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

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

TEST(ScsiControllerTest, DataOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

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

TEST(ScsiControllerTest, RequestSense)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    auto controller = make_shared<MockScsiController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);
    EXPECT_TRUE(device->Init( { }));

    controller->AddDevice(device);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    // Non-existing LUN
    controller->SetCdbByte(1, 0x20);

    device->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::cmd_request_sense));
    EXPECT_EQ(status::good, controller->GetStatus()) << "Wrong CHECK CONDITION for non-existing LUN";
}
