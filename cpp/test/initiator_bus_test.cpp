//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(InitiatorBusTest, InitiatorReceiveHandShake)
{
    MockBus bus;
    array<uint8_t, 1> buf = { };

    EXPECT_CALL(bus, Acquire);
    EXPECT_CALL(bus, EnableIRQ);
    EXPECT_CALL(bus, DisableIRQ);
    EXPECT_CALL(bus, WaitHandShake);
    EXPECT_EQ(0, bus.InitiatorReceiveHandShake(buf));
}

TEST(InitiatorBusTest, InitiatorSendHandShake)
{
    MockBus bus;
    array<uint8_t, 1> buf = { };

    EXPECT_CALL(bus, SetDAT);
    EXPECT_CALL(bus, WaitNanoSeconds);
    EXPECT_CALL(bus, Acquire);
    EXPECT_CALL(bus, EnableIRQ);
    EXPECT_CALL(bus, DisableIRQ);
    EXPECT_CALL(bus, WaitHandShake);
    EXPECT_EQ(0, bus.InitiatorSendHandShake(buf));
}
