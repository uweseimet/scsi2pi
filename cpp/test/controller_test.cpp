//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_defs.h"
#include "buses/bus_factory.h"
#include "shared/s2p_exceptions.h"

TEST(ControllerTest, Reset)
{
    const int TARGET_ID = 5;
    const int INITIATOR_ID = 7;

    NiceMock<MockBus> bus;
    const S2pFormatter formatter;
    auto controller = make_shared<Controller>(bus, TARGET_ID, formatter);

    controller->Init();

    controller->ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller->GetInitiatorId());
    controller->Reset();
    EXPECT_EQ(-1, controller->GetInitiatorId());
}

TEST(ControllerTest, Process)
{
    const S2pFormatter formatter;
    auto bus = bus_factory::CreateBus(true, true, "", false);
    auto controller = make_shared<Controller>(*bus, 2, formatter);

    bus->SetRST(true);
    EXPECT_FALSE(controller->Process());
}

TEST(ControllerTest, GetInitiatorId)
{
    const int TARGET_ID = 0;
    const int INITIATOR_ID = 2;

    NiceMock<MockBus> bus;
    const S2pFormatter formatter;
    auto controller = make_shared<Controller>(bus, TARGET_ID, formatter);

    controller->Init();

    controller->ProcessOnController((1 << TARGET_ID) + (1 << INITIATOR_ID));
    EXPECT_EQ(INITIATOR_ID, controller->GetInitiatorId());
}

TEST(ControllerTest, BusFree)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(BusPhase::BUS_FREE);
    controller.BusFree();
    EXPECT_EQ(BusPhase::BUS_FREE, controller.GetPhase());

    controller.SetStatus(StatusCode::CHECK_CONDITION);
    controller.SetPhase(BusPhase::RESERVED);
    controller.BusFree();
    EXPECT_EQ(BusPhase::BUS_FREE, controller.GetPhase());
    EXPECT_EQ(StatusCode::GOOD, controller.GetStatus());

    controller.ScheduleShutdown(ShutdownMode::NONE);
    controller.SetPhase(BusPhase::RESERVED);
    controller.BusFree();

    controller.ScheduleShutdown(ShutdownMode::STOP_PI);
    controller.SetPhase(BusPhase::RESERVED);
    controller.BusFree();

    controller.ScheduleShutdown(ShutdownMode::RESTART_PI);
    controller.SetPhase(BusPhase::RESERVED);
    controller.BusFree();

    controller.ScheduleShutdown(ShutdownMode::STOP_S2P);
    controller.SetPhase(BusPhase::RESERVED);
    controller.BusFree();
}

TEST(ControllerTest, Selection)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    auto controller = make_shared<MockController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->AddDevice(device);

    controller->SetPhase(BusPhase::SELECTION);
    controller->Selection();
    EXPECT_EQ(BusPhase::SELECTION, controller->GetPhase());

    controller->Selection();
    EXPECT_EQ(BusPhase::SELECTION, controller->GetPhase());

    ON_CALL(*bus, GetDAT).WillByDefault(Return(1));
    controller->Selection();
    EXPECT_EQ(BusPhase::SELECTION, controller->GetPhase());
}

TEST(ControllerTest, Command)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.AddDevice(device);

    controller.SetPhase(BusPhase::COMMAND);
    EXPECT_CALL(controller, Status).Times(2);
    controller.Command();
    EXPECT_EQ(BusPhase::COMMAND, controller.GetPhase());

    controller.SetPhase(BusPhase::RESERVED);
    controller.Command();
    EXPECT_EQ(BusPhase::COMMAND, controller.GetPhase());

    controller.SetPhase(BusPhase::RESERVED);
    controller.Command();
    EXPECT_EQ(BusPhase::COMMAND, controller.GetPhase());
}

TEST(ControllerTest, MsgIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(BusPhase::RESERVED);
    controller.MsgIn();
    EXPECT_EQ(BusPhase::MSG_IN, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(ControllerTest, MsgOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(BusPhase::RESERVED);
    controller.MsgOut();
    EXPECT_EQ(BusPhase::MSG_OUT, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
    EXPECT_EQ(1, controller.GetCurrentLength());
}

TEST(ControllerTest, DataIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(BusPhase::RESERVED);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataIn();
    EXPECT_EQ(BusPhase::RESERVED, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataIn();
    EXPECT_EQ(BusPhase::DATA_IN, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ControllerTest, DataOut)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockController controller(bus);

    controller.SetPhase(BusPhase::RESERVED);
    controller.SetCurrentLength(0);
    EXPECT_CALL(controller, Status);
    controller.DataOut();
    EXPECT_EQ(BusPhase::RESERVED, controller.GetPhase());

    controller.SetCurrentLength(1);
    controller.DataOut();
    EXPECT_EQ(BusPhase::DATA_OUT, controller.GetPhase());
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
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::REQUEST_SENSE));
    EXPECT_EQ(StatusCode::GOOD, controller.GetStatus()) << "Wrong CHECK CONDITION for non-existing LUN";
}
