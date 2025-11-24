//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(InProcessBusTest, IsTarget)
{
    MockInProcessBus bus;

    bus.Init(true);
    EXPECT_TRUE(bus.IsTarget());
    bus.Init(false);
    EXPECT_FALSE(bus.IsTarget());
}

TEST(InProcessBusTest, BSY)
{
    MockInProcessBus bus;

    bus.SetBSY(true);
    EXPECT_TRUE(bus.GetBSY());
    bus.SetBSY(false);
    EXPECT_FALSE(bus.GetBSY());
}

TEST(InProcessBusTest, SEL)
{
    MockInProcessBus bus;

    bus.SetSEL(true);
    EXPECT_TRUE(bus.GetSEL());
    bus.SetSEL(false);
    EXPECT_FALSE(bus.GetSEL());
}

TEST(InProcessBusTest, ATN)
{
    MockInProcessBus bus;

    bus.SetATN(true);
    EXPECT_TRUE(bus.GetATN());
    bus.SetATN(false);
    EXPECT_FALSE(bus.GetATN());
}

TEST(InProcessBusTest, ACK)
{
    MockInProcessBus bus;

    bus.SetACK(true);
    EXPECT_TRUE(bus.GetACK());
    bus.SetACK(false);
    EXPECT_FALSE(bus.GetACK());
}

TEST(InProcessBusTest, REQ)
{
    MockInProcessBus bus;

    bus.SetREQ(true);
    EXPECT_TRUE(bus.GetREQ());
    bus.SetREQ(false);
    EXPECT_FALSE(bus.GetREQ());
}

TEST(InProcessBusTest, RST)
{
    MockInProcessBus bus;

    bus.SetRST(true);
    EXPECT_TRUE(bus.GetRST());
    bus.SetRST(false);
    EXPECT_FALSE(bus.GetRST());
}

TEST(InProcessBusTest, MSG)
{
    MockInProcessBus bus;

    bus.SetMSG(true);
    EXPECT_TRUE(bus.GetMSG());
    bus.SetMSG(false);
    EXPECT_FALSE(bus.GetMSG());
}

TEST(InProcessBusTest, CD)
{
    MockInProcessBus bus;

    bus.SetCD(true);
    EXPECT_TRUE(bus.GetCD());
    bus.SetCD(false);
    EXPECT_FALSE(bus.GetCD());
}

TEST(InProcessBusTest, IO)
{
    MockInProcessBus bus;

    bus.SetIO(true);
    EXPECT_TRUE(bus.GetIO());
    bus.SetIO(false);
    EXPECT_FALSE(bus.GetIO());
}

TEST(InProcessBusTest, DAT)
{
    MockInProcessBus bus;

    bus.SetDAT(0xae);
    EXPECT_EQ(0xae, bus.GetDAT());
    bus.SetDAT(0x21);
    EXPECT_EQ(0x21, bus.GetDAT());
}

TEST(InProcessBusTest, Acquire)
{
    MockInProcessBus bus;

    bus.SetDAT(0x12);
    EXPECT_EQ(0x12U, bus.Acquire());
}

TEST(InProcessBusTest, Reset)
{
    MockInProcessBus bus;

    bus.SetSignal(PIN_BSY, true);
    EXPECT_TRUE(bus.GetSignal(PIN_BSY_MASK));
    bus.ResetMock();
    EXPECT_FALSE(bus.GetSignal(PIN_BSY_MASK));
}

TEST(InProcessBusTest, SetGetSignal)
{
    MockInProcessBus bus;

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.GetSignal(PIN_REQ_MASK));
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.GetSignal(PIN_REQ_MASK));
}

TEST(InProcessBusTest, WaitHandshakeSignal)
{
    MockInProcessBus bus;

    bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(bus.WaitHandshakeSignal(PIN_ACK_MASK, true));

    bus.SetSignal(PIN_ACK, false);
    bus.SetSignal(PIN_RST, true);
    EXPECT_FALSE(bus.WaitHandshakeSignal(PIN_ACK_MASK, true));
}

TEST(InProcessBusTest, WaitForSelection)
{
    MockInProcessBus bus;

    EXPECT_TRUE(bus.WaitForSelection());
}

TEST(DelegatingProcessBusTest, Reset)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", false);

    EXPECT_CALL(bus, Reset);
    delegating_bus.Reset();
}

TEST(DelegatingProcessBusTest, Acquire)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", false);

    bus.SetDAT(0x45);
    EXPECT_EQ(0x45U, delegating_bus.Acquire());
}

TEST(DelegatingProcessBusTest, SetGetSignal)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", true);

    delegating_bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(delegating_bus.GetSignal(PIN_ACK_MASK));
    delegating_bus.SetSignal(PIN_ACK, false);
    EXPECT_FALSE(delegating_bus.GetSignal(PIN_ACK_MASK));

    delegating_bus.SetSignal(PIN_IO, true);
    EXPECT_TRUE(delegating_bus.GetSignal(PIN_IO_MASK));
    delegating_bus.SetSignal(PIN_IO, false);
    EXPECT_FALSE(delegating_bus.GetSignal(PIN_IO_MASK));
}

TEST(DelegatingProcessBusTest, WaitHandshakeSignal)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", false);

    bus.SetACK(true);
    EXPECT_TRUE(delegating_bus.WaitHandshakeSignal(PIN_ACK_MASK, true));
    bus.SetACK(false);
    EXPECT_TRUE(delegating_bus.WaitHandshakeSignal(PIN_ACK_MASK, false));

    bus.SetREQ(true);
    EXPECT_TRUE(delegating_bus.WaitHandshakeSignal(PIN_REQ_MASK, true));
    bus.SetREQ(false);
    EXPECT_TRUE(delegating_bus.WaitHandshakeSignal(PIN_REQ_MASK, false));
}

TEST(DelegatingProcessBusTest, DAT)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", false);

    delegating_bus.SetDAT(0x56);
    EXPECT_EQ(0x56, delegating_bus.GetDAT());
    EXPECT_EQ(0x56, bus.GetDAT());
}

TEST(DelegatingProcessBusTest, CleanUp)
{
    MockInProcessBus bus;
    DelegatingInProcessBus delegating_bus(bus, "", false);

    EXPECT_CALL(bus, CleanUp);
    delegating_bus.CleanUp();
}
