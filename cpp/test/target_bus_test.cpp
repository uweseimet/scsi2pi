//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(TargetBusTest, TargetCommandHandShake)
{
    MockBus bus;
    array<uint8_t, 1> buf = { };

    EXPECT_CALL(bus, SetSignal).Times(2);
    EXPECT_CALL(bus, Acquire);
    EXPECT_CALL(bus, EnableIRQ);
    EXPECT_CALL(bus, DisableIRQ);
    EXPECT_CALL(bus, WaitHandShake);
    EXPECT_CALL(bus, WaitNanoSeconds);
    EXPECT_EQ(-1, bus.TargetCommandHandShake(buf));
}

TEST(TargetBusTest, TargetReceiveHandShake)
{
    MockBus bus;
    array<uint8_t, 1> buf = { };

    EXPECT_CALL(bus, SetSignal).Times(2);
    EXPECT_CALL(bus, Acquire);
    EXPECT_CALL(bus, EnableIRQ);
    EXPECT_CALL(bus, DisableIRQ);
    EXPECT_CALL(bus, WaitHandShake);
    EXPECT_CALL(bus, WaitNanoSeconds);
    EXPECT_EQ(0, bus.TargetReceiveHandShake(buf));
}

TEST(TargetBusTest, TargetSendHandShake)
{
    MockBus bus;
    array<uint8_t, 1> buf = { };

    EXPECT_CALL(bus, SetDAT);
    EXPECT_CALL(bus, WaitNanoSeconds);
    EXPECT_CALL(bus, EnableIRQ);
    EXPECT_CALL(bus, DisableIRQ);
    EXPECT_CALL(bus, WaitHandShake);
    EXPECT_EQ(0, bus.TargetSendHandShake(buf));
}
