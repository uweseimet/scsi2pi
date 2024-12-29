//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_defs.h"
#include "shared/s2p_exceptions.h"

TEST(AbstractControllerTest, ShutdownMode)
{
    MockAbstractController controller;

    EXPECT_CALL(controller, Process);
    EXPECT_EQ(ShutdownMode::NONE, controller.ProcessOnController(0));
    controller.ScheduleShutdown(ShutdownMode::STOP_S2P);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(ShutdownMode::STOP_S2P, controller.ProcessOnController(0));
    controller.ScheduleShutdown(ShutdownMode::STOP_PI);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(ShutdownMode::STOP_PI, controller.ProcessOnController(0));
    controller.ScheduleShutdown(ShutdownMode::RESTART_PI);
    EXPECT_CALL(controller, Process);
    EXPECT_EQ(ShutdownMode::RESTART_PI, controller.ProcessOnController(0));
}

TEST(AbstractControllerTest, SetCurrentLength)
{
    MockAbstractController controller;

    EXPECT_EQ(512U, controller.GetBuffer().size());
    controller.SetCurrentLength(1);
    EXPECT_LE(1U, controller.GetBuffer().size());
    controller.SetCurrentLength(10000);
    EXPECT_LE(10000U, controller.GetBuffer().size());
}

TEST(AbstractControllerTest, Reset)
{
    const auto bus = make_shared<MockBus>();
    MockAbstractController controller(bus, 0);

    controller.AddDevice(make_shared<MockPrimaryDevice>(0));

    controller.SetPhase(BusPhase::STATUS);
    EXPECT_EQ(BusPhase::STATUS, controller.GetPhase());
    EXPECT_CALL(*bus, Reset);
    controller.Reset();
    EXPECT_TRUE(controller.IsBusFree());
    EXPECT_EQ(StatusCode::GOOD, controller.GetStatus());
    EXPECT_EQ(0, controller.GetCurrentLength());
}

TEST(AbstractControllerTest, Status)
{
    MockAbstractController controller;

    controller.SetStatus(StatusCode::RESERVATION_CONFLICT);
    EXPECT_EQ(StatusCode::RESERVATION_CONFLICT, controller.GetStatus());
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
    const int ID = 1;
    const int LUN = 4;

    MockAbstractController controller(ID);

    const auto device = make_shared<MockPrimaryDevice>(LUN);

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

TEST(AbstractControllerTest, AddDevice)
{
    MockAbstractController controller;

    EXPECT_TRUE(controller.AddDevice(make_shared<MockPrimaryDevice>(0)));
    EXPECT_TRUE(controller.AddDevice(make_shared<MockScsiHd>(1, false)));
    EXPECT_FALSE(controller.AddDevice(make_shared<MockSasiHd>(2)));
}

TEST(AbstractControllerTest, Lengths)
{
    MockAbstractController controller;

    controller.SetTransferSize(3, 1);
    EXPECT_EQ(3, controller.GetRemainingLength());
    EXPECT_EQ(1, controller.GetChunkSize());
    controller.UpdateTransferLength(1);
    EXPECT_EQ(2, controller.GetRemainingLength());
    EXPECT_EQ(1, controller.GetChunkSize());
    controller.UpdateTransferLength(1);
    EXPECT_EQ(1, controller.GetRemainingLength());
    EXPECT_EQ(1, controller.GetChunkSize());
    controller.UpdateTransferLength(1);
    EXPECT_EQ(0, controller.GetRemainingLength());
    EXPECT_EQ(0, controller.GetChunkSize());
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

    EXPECT_CALL(controller, Process);
    controller.ProcessOnController(0x02);
    EXPECT_CALL(controller, Process);
    controller.ProcessOnController(0x06);
}

TEST(AbstractControllerTest, ScriptGenerator)
{
    NiceMock<MockAbstractController> controller;
    auto generator = make_shared<ScriptGenerator>();
    controller.SetScriptGenerator(generator);
    const string &filename = CreateTempFile();
    generator->CreateFile(filename);

    controller.AddCdbToScript();
    array<uint8_t, 1> data = { };
    controller.AddDataToScript(data);
    ifstream file(filename);
    string s;
    getline(file, s);
    EXPECT_EQ(s, "-i 0:0 -c 00:00:00:00:00:00 -d 00");
}
