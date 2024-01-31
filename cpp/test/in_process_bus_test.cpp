//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(InProcessBusTest, IsTarget)
{
    MockInProcessBus bus;

    bus.Init(false);
    EXPECT_FALSE(bus.IsTarget());
    bus.Init(true);
    EXPECT_TRUE(bus.IsTarget());
}

TEST(InProcessBusTest, BSY)
{
    InProcessBus bus;

    bus.SetBSY(true);
    EXPECT_TRUE(bus.GetBSY());
    bus.SetBSY(false);
    EXPECT_FALSE(bus.GetBSY());
}

TEST(InProcessBusTest, SEL)
{
    InProcessBus bus;

    bus.SetSEL(true);
    EXPECT_TRUE(bus.GetSEL());
    bus.SetSEL(false);
    EXPECT_FALSE(bus.GetSEL());
}

TEST(InProcessBusTest, ATN)
{
    InProcessBus bus;

    bus.SetATN(true);
    EXPECT_TRUE(bus.GetATN());
    bus.SetATN(false);
    EXPECT_FALSE(bus.GetATN());
}

TEST(InProcessBusTest, ACK)
{
    InProcessBus bus;

    bus.SetACK(true);
    EXPECT_TRUE(bus.GetACK());
    bus.SetACK(false);
    EXPECT_FALSE(bus.GetACK());
}

TEST(InProcessBusTest, REQ)
{
    InProcessBus bus;

    bus.SetREQ(true);
    EXPECT_TRUE(bus.GetREQ());
    bus.SetREQ(false);
    EXPECT_FALSE(bus.GetREQ());
}

TEST(InProcessBusTest, RST)
{
    InProcessBus bus;

    bus.SetRST(true);
    EXPECT_TRUE(bus.GetRST());
    bus.SetRST(false);
    EXPECT_FALSE(bus.GetRST());
}

TEST(InProcessBusTest, MSG)
{
    InProcessBus bus;

    bus.SetMSG(true);
    EXPECT_TRUE(bus.GetMSG());
    bus.SetMSG(false);
    EXPECT_FALSE(bus.GetMSG());
}

TEST(InProcessBusTest, CD)
{
    InProcessBus bus;

    bus.SetCD(true);
    EXPECT_TRUE(bus.GetCD());
    bus.SetCD(false);
    EXPECT_FALSE(bus.GetCD());
}

TEST(InProcessBusTest, IO)
{
    InProcessBus bus;

    bus.SetIO(true);
    EXPECT_TRUE(bus.GetIO());
    bus.SetIO(false);
    EXPECT_FALSE(bus.GetIO());
}

TEST(InProcessBusTest, DAT)
{
    InProcessBus bus;

    bus.SetDAT(0xae);
    EXPECT_EQ(0xae, bus.GetDAT());
    bus.SetDAT(0x21);
    EXPECT_EQ(0x21, bus.GetDAT());
}

TEST(InProcessBusTest, Acquire)
{
    InProcessBus bus;

    bus.SetDAT(0x12);
    EXPECT_EQ(0x12U, bus.Acquire());
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

TEST(DelegatingProcessBusTest, Reset)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    EXPECT_CALL(bus, Reset());
    delegating_bus.Reset();
}

TEST(DelegatingProcessBusTest, Acquire)
{
    InProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    bus.SetDAT(0x45);
    EXPECT_EQ(0x45U, delegating_bus.Acquire());
}

TEST(DelegatingProcessBusTest, WaitACK)
{
    InProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    bus.SetACK(true);
    EXPECT_TRUE(delegating_bus.WaitACK(true));
    bus.SetACK(false);
    EXPECT_TRUE(delegating_bus.WaitACK(false));
}

TEST(DelegatingProcessBusTest, WaitREQ)
{
    InProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    bus.SetREQ(true);
    EXPECT_TRUE(delegating_bus.WaitREQ(true));
    bus.SetREQ(false);
    EXPECT_TRUE(delegating_bus.WaitREQ(false));
}

TEST(DelegatingProcessBusTest, DAT)
{
    InProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    delegating_bus.SetDAT(0x56);
    EXPECT_EQ(0x56, delegating_bus.GetDAT());
    EXPECT_EQ(0x56, bus.GetDAT());
}

TEST(DelegatingProcessBusTest, CleanUp)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, false);

    EXPECT_CALL(bus, CleanUp());
    delegating_bus.CleanUp();
}
