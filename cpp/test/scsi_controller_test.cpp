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
    auto controller = make_shared<ScsiController>(bus, ID, 32);
    auto device = make_shared<MockPrimaryDevice>(0);

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

    controller.AddDevice(device);

    EXPECT_CALL(controller, Status).Times(2);
    controller.Process(ID);
    EXPECT_EQ(ID, controller.GetInitiatorId());
    controller.Process(1234);
    EXPECT_EQ(1234, controller.GetInitiatorId());
}

TEST(ScsiControllerTest, Process)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.Init();
    controller.AddDevice(device);

    controller.SetPhase(phase_t::reserved);
    ON_CALL(*bus, GetRST).WillByDefault(Return(true));
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST);
    EXPECT_CALL(controller, Reset());
    EXPECT_FALSE(controller.Process(0));

    controller.SetPhase(phase_t::busfree);
    ON_CALL(*bus, GetRST).WillByDefault(Return(false));
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST);
    EXPECT_CALL(controller, Status());
    EXPECT_FALSE(controller.Process(0));

    controller.SetPhase(phase_t::reserved);
    EXPECT_CALL(*bus, Acquire).Times(2);
    EXPECT_CALL(*bus, GetRST).Times(2);
    EXPECT_FALSE(controller.Process(0));
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
    ON_CALL(*bus, GetSEL).WillByDefault(Return(true));
    ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
    EXPECT_CALL(*bus, GetATN).Times(0);
    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());

    ON_CALL(*bus, GetSEL).WillByDefault(Return(true));
    ON_CALL(*bus, GetBSY).WillByDefault(Return(false));
    EXPECT_CALL(*bus, GetATN).Times(0);
    EXPECT_CALL(*controller, Status);
    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());

    ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
    ON_CALL(*bus, GetBSY).WillByDefault(Return(false));
    EXPECT_CALL(*bus, GetATN).Times(0);
    controller->Selection();
    EXPECT_EQ(phase_t::selection, controller->GetPhase());

    ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
    ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
    ON_CALL(*bus, GetATN).WillByDefault(Return(false));
    EXPECT_CALL(*bus, GetATN);
    controller->Selection();
    EXPECT_EQ(phase_t::command, controller->GetPhase());

    controller->SetPhase(phase_t::selection);
    ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
    ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
    ON_CALL(*bus, GetATN).WillByDefault(Return(true));
    EXPECT_CALL(*bus, GetATN);
    controller->Selection();
    EXPECT_EQ(phase_t::msgout, controller->GetPhase());

    ON_CALL(*bus, GetDAT).WillByDefault(Return(1));
    EXPECT_CALL(*bus, SetBSY(true));
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
    EXPECT_CALL(*bus, SetMSG(false));
    EXPECT_CALL(*bus, SetCD(true));
    EXPECT_CALL(*bus, SetIO(false));
    controller.Command();
    EXPECT_EQ(phase_t::command, controller.GetPhase());

    controller.SetPhase(phase_t::reserved);
    EXPECT_CALL(*bus, SetMSG(false));
    EXPECT_CALL(*bus, SetCD(true));
    EXPECT_CALL(*bus, SetIO(false));
    controller.Command();
    EXPECT_EQ(phase_t::command, controller.GetPhase());
}

TEST(ScsiControllerTest, MsgIn)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);

    controller.SetPhase(phase_t::reserved);
    EXPECT_CALL(*bus, SetMSG(true));
    EXPECT_CALL(*bus, SetCD(true));
    EXPECT_CALL(*bus, SetIO(true));
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
    EXPECT_CALL(*bus, SetMSG(true));
    EXPECT_CALL(*bus, SetCD(true));
    EXPECT_CALL(*bus, SetIO(false));
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
    EXPECT_CALL(*bus, SetMSG(false));
    EXPECT_CALL(*bus, SetCD(false));
    EXPECT_CALL(*bus, SetIO(true));
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
    EXPECT_CALL(*bus, SetMSG(false));
    EXPECT_CALL(*bus, SetCD(false));
    EXPECT_CALL(*bus, SetIO(false));
    controller.DataOut();
    EXPECT_EQ(phase_t::dataout, controller.GetPhase());
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(ScsiControllerTest, Error)
{
    auto bus = make_shared<NiceMock<MockBus>>();
    MockScsiController controller(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller.AddDevice(device);

    ON_CALL(*bus, GetRST).WillByDefault(Return(true));
    controller.SetPhase(phase_t::reserved);
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST());
    EXPECT_CALL(controller, Reset).Times(0);
    controller.Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
    EXPECT_EQ(status::good, controller.GetStatus());
    EXPECT_EQ(phase_t::busfree, controller.GetPhase());

    ON_CALL(*bus, GetRST).WillByDefault(Return(false));
    controller.SetPhase(phase_t::status);
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST());
    EXPECT_CALL(controller, Reset).Times(0);
    controller.Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
    EXPECT_EQ(phase_t::busfree, controller.GetPhase());

    controller.SetPhase(phase_t::msgin);
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST());
    EXPECT_CALL(controller, Reset).Times(0);
    controller.Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
    EXPECT_EQ(phase_t::busfree, controller.GetPhase());

    controller.SetPhase(phase_t::reserved);
    EXPECT_CALL(*bus, Acquire);
    EXPECT_CALL(*bus, GetRST());
    EXPECT_CALL(controller, Reset).Times(0);
    EXPECT_CALL(controller, Status);
    controller.Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
    EXPECT_EQ(status::reservation_conflict, controller.GetStatus());
    EXPECT_EQ(phase_t::reserved, controller.GetPhase());
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
