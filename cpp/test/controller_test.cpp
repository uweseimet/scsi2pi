//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/s2p_defs.h"
#include "buses/bus_factory.h"
#include "shared/s2p_exceptions.h"

TEST(ControllerTest, Reset)
{
    const int TARGET_ID = 5;
    const int INITIATOR_ID = 7;

    const S2pFormatter formatter;
    auto bus = BusFactory::Instance().CreateBus(true, true, "");
    auto controller = make_shared<Controller>(*bus, TARGET_ID, formatter);

    controller->Init();

    controller->ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller->GetInitiatorId());
    controller->Reset();
    EXPECT_EQ(-1, controller->GetInitiatorId());
}

TEST(ControllerTest, Process)
{
    const S2pFormatter formatter;
    auto bus = BusFactory::Instance().CreateBus(true, true, "");
    auto controller = make_shared<Controller>(*bus, 2, formatter);

    bus->SetRST(true);
    EXPECT_FALSE(controller->Process());
}

TEST(ControllerTest, GetInitiatorId)
{
    const int TARGET_ID = 0;
    const int INITIATOR_ID = 2;

    const S2pFormatter formatter;
    auto bus = BusFactory::Instance().CreateBus(true, true, "");
    auto controller = make_shared<Controller>(*bus, TARGET_ID, formatter);

    controller->Init();

    controller->ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller->GetInitiatorId());
}

TEST(ControllerTest, BusFree)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(bus_phase::busfree);
    controller.BusFree();
    EXPECT_EQ(bus_phase::busfree, controller.GetPhase());

    controller.SetStatus(status_code::check_condition);
    controller.SetPhase(bus_phase::reserved);
    controller.BusFree();
    EXPECT_EQ(bus_phase::busfree, controller.GetPhase());
    EXPECT_EQ(status_code::good, controller.GetStatus());

    controller.ScheduleShutdown(shutdown_mode::none);
    controller.SetPhase(bus_phase::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(shutdown_mode::stop_pi);
    controller.SetPhase(bus_phase::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(shutdown_mode::restart_pi);
    controller.SetPhase(bus_phase::reserved);
    controller.BusFree();

    controller.ScheduleShutdown(shutdown_mode::stop_s2p);
    controller.SetPhase(bus_phase::reserved);
    controller.BusFree();
}

TEST(ControllerTest, Selection)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    auto controller = make_shared<MockController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->AddDevice(device);

    controller->SetPhase(bus_phase::selection);
    controller->Selection();
    EXPECT_EQ(bus_phase::selection, controller->GetPhase());

    controller->Selection();
    EXPECT_EQ(bus_phase::selection, controller->GetPhase());

    ON_CALL(*bus, GetDAT).WillByDefault(Return(1));
    controller->Selection();
    EXPECT_EQ(bus_phase::selection, controller->GetPhase());
}

TEST(ControllerTest, Command)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.AddDevice(device);

    controller.SetPhase(bus_phase::command);
    EXPECT_CALL(controller, Status).Times(2);
    controller.Command();
    EXPECT_EQ(bus_phase::command, controller.GetPhase());

    controller.SetPhase(bus_phase::reserved);
    controller.Command();
    EXPECT_EQ(bus_phase::command, controller.GetPhase());

    controller.SetPhase(bus_phase::reserved);
    controller.Command();
    EXPECT_EQ(bus_phase::command, controller.GetPhase());
}

TEST(ControllerTest, MsgIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(bus_phase::reserved);
    controller.MsgIn();
    EXPECT_EQ(bus_phase::msgin, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(ControllerTest, MsgOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(bus_phase::reserved);
    controller.MsgOut();
    EXPECT_EQ(bus_phase::msgout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(1, controller.GetCurrentLength());
}

TEST(ControllerTest, DataIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(bus_phase::reserved);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataIn();
    EXPECT_EQ(bus_phase::reserved, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataIn();
    EXPECT_EQ(bus_phase::datain, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ControllerTest, DataOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(bus_phase::reserved);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataOut();
    EXPECT_EQ(bus_phase::reserved, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataOut();
    EXPECT_EQ(bus_phase::dataout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ControllerTest, RequestSense)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);
    auto device = make_shared<MockPrimaryDevice>(0);
    EXPECT_EQ("", device->Init());

    controller.AddDevice(device);

    // ALLOCATION LENGTH
    controller.SetCdbByte(4, 255);
    // Non-existing LUN
    controller.SetCdbByte(1, 0x20);

    device->SetReady(true);
    EXPECT_CALL(controller, Status);
    EXPECT_NO_THROW(Dispatch(device, scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller.GetStatus()) << "Wrong CHECK CONDITION for non-existing LUN";
}

TEST(ControllerTest, GetLunMax)
{
    EXPECT_EQ(32, Controller::GetLunMax(false));
    EXPECT_EQ(2, Controller::GetLunMax(true));
}
