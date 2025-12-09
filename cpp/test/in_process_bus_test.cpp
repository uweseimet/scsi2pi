//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

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
    bus.Acquire();
    EXPECT_EQ(0x12U, bus.GetDAT());
}

TEST(InProcessBusTest, BusPhases)
{
    MockInProcessBus bus;

    EXPECT_EQ(BusPhase::BUS_FREE, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::BUS_FREE));

    bus.SetBSY(true);

    bus.SetIO(true);
    bus.SetCD(true);
    bus.SetMSG(true);
    EXPECT_EQ(BusPhase::MSG_IN, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::MSG_IN));

    bus.SetIO(true);
    bus.SetCD(true);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::STATUS, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::STATUS));

    bus.SetIO(true);
    bus.SetCD(false);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::DATA_IN, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::DATA_IN));

    bus.SetIO(false);
    bus.SetCD(true);
    bus.SetMSG(true);
    EXPECT_EQ(BusPhase::MSG_OUT, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::MSG_OUT));

    bus.SetIO(false);
    bus.SetCD(true);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::COMMAND, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::COMMAND));

    bus.SetIO(false);
    bus.SetCD(false);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::DATA_OUT, bus.GetPhase());
    EXPECT_TRUE(bus.IsPhase(BusPhase::DATA_OUT));
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

TEST(InProcessBusTest, WaitHandshakeACK)
{
    MockInProcessBus bus;

    bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(bus.WaitHandShake(PIN_ACK_MASK, true));

    bus.SetSignal(PIN_ACK, false);
    EXPECT_TRUE(bus.WaitHandShake(PIN_ACK_MASK, false));

    bus.SetSignal(PIN_RST, true);
    bus.SetSignal(PIN_ACK, false);
    EXPECT_FALSE(bus.WaitHandShake(PIN_ACK_MASK, true));
}

TEST(InProcessBusTest, WaitHandshakeREQ)
{
    MockInProcessBus bus;

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.WaitHandShake(PIN_REQ_MASK, true));

    bus.SetSignal(PIN_REQ, false);
    EXPECT_TRUE(bus.WaitHandShake(PIN_REQ_MASK, false));

    bus.SetSignal(PIN_RST, true);
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.WaitHandShake(PIN_REQ_MASK, true));
}

TEST(InProcessBusTest, GetSelection)
{
    MockInProcessBus bus;

    bus.SetDAT(0x00);
    bus.SetBSY(false);
    EXPECT_EQ(0x00, bus.GetSelection());
}

TEST(InProcessBusTest, WaitForSelection)
{
    MockInProcessBus bus;

    bus.SetDAT(0x40);
    bus.SetBSY(false);
    EXPECT_EQ(0x40, bus.WaitForSelection());
}

TEST(InProcessBusTest, IsRaspberryPi)
{
    MockInProcessBus bus;

    EXPECT_FALSE(bus.IsRaspberryPi());
}
