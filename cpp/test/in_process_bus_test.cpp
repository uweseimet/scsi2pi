//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "buses/in_process_bus.h"

TEST(InProcessBusTest, IsTarget)
{
    MockInProcessBus bus;

    EXPECT_TRUE(bus.IsTarget());
}

TEST(InProcessBusTest, Acquire)
{
    InProcessBus bus;

    bus.SetDAT(0x12);
    EXPECT_EQ(0x12, bus.Acquire());
}

TEST(InProcessBusTest, Reset)
{
    InProcessBus bus;

    bus.SetSignal(PIN_BSY, true);
    EXPECT_TRUE(bus.GetSignal(PIN_BSY));
    bus.Reset();
    EXPECT_FALSE(bus.GetSignal(PIN_BSY));
}

TEST(InProcessBusTest, SetGetSignal)
{
    InProcessBus bus;

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.GetSignal(PIN_REQ));
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.GetSignal(PIN_REQ));
}

TEST(InProcessBusTest, WaitSignal)
{
    InProcessBus bus;

    bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(bus.WaitSignal(PIN_ACK, true));

    bus.SetSignal(PIN_ACK, false);
    bus.SetSignal(PIN_RST, true);
    EXPECT_FALSE(bus.WaitSignal(PIN_ACK, true));
}

TEST(InProcessBusTest, WaitForSelection)
{
    InProcessBus bus;

    EXPECT_TRUE(bus.WaitForSelection());
}
