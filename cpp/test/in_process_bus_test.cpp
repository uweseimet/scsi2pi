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
    InProcessBus bus("", true);

    bus.SetBSY(true);
    EXPECT_TRUE(bus.GetBSY());
    bus.SetBSY(false);
    EXPECT_FALSE(bus.GetBSY());
}

TEST(InProcessBusTest, SEL)
{
    InProcessBus bus("", true);

    bus.SetSEL(true);
    EXPECT_TRUE(bus.GetSEL());
    bus.SetSEL(false);
    EXPECT_FALSE(bus.GetSEL());
}

TEST(InProcessBusTest, ATN)
{
    InProcessBus bus("", true);

    bus.SetATN(true);
    EXPECT_TRUE(bus.GetATN());
    bus.SetATN(false);
    EXPECT_FALSE(bus.GetATN());
}

TEST(InProcessBusTest, ACK)
{
    InProcessBus bus("", true);

    bus.SetACK(true);
    EXPECT_TRUE(bus.GetACK());
    bus.SetACK(false);
    EXPECT_FALSE(bus.GetACK());
}

TEST(InProcessBusTest, REQ)
{
    InProcessBus bus("", true);

    bus.SetREQ(true);
    EXPECT_TRUE(bus.GetREQ());
    bus.SetREQ(false);
    EXPECT_FALSE(bus.GetREQ());
}

TEST(InProcessBusTest, RST)
{
    InProcessBus bus("", false);

    bus.SetRST(true);
    EXPECT_TRUE(bus.GetRST());
    bus.SetRST(false);
    EXPECT_FALSE(bus.GetRST());
}

TEST(InProcessBusTest, MSG)
{
    InProcessBus bus("", false);

    bus.SetMSG(true);
    EXPECT_TRUE(bus.GetMSG());
    bus.SetMSG(false);
    EXPECT_FALSE(bus.GetMSG());
}

TEST(InProcessBusTest, CD)
{
    InProcessBus bus("", false);

    bus.SetCD(true);
    EXPECT_TRUE(bus.GetCD());
    bus.SetCD(false);
    EXPECT_FALSE(bus.GetCD());
}

TEST(InProcessBusTest, IO)
{
    InProcessBus bus("", true);

    bus.SetIO(true);
    EXPECT_TRUE(bus.GetIO());
    bus.SetIO(false);
    EXPECT_FALSE(bus.GetIO());
}

TEST(InProcessBusTest, DAT)
{
    InProcessBus bus("", false);

    bus.SetDAT(0xae);
    EXPECT_EQ(0xae, bus.GetDAT());
    bus.SetDAT(0x21);
    EXPECT_EQ(0x21, bus.GetDAT());
}

TEST(InProcessBusTest, Acquire)
{
    InProcessBus bus("", false);

    bus.SetDAT(0x12);
    EXPECT_EQ(0x12U, bus.Acquire());
}

TEST(InProcessBusTest, BusPhases)
{
    InProcessBus bus("", false);

    EXPECT_EQ(BusPhase::BUS_FREE, bus.GetPhase());

    bus.SetBSY(true);

    bus.SetIO(true);
    bus.SetCD(true);
    bus.SetMSG(true);
    EXPECT_EQ(BusPhase::MSG_IN, bus.GetPhase());

    bus.SetIO(true);
    bus.SetCD(true);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::STATUS, bus.GetPhase());

    bus.SetIO(true);
    bus.SetCD(false);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::DATA_IN, bus.GetPhase());

    bus.SetIO(false);
    bus.SetCD(true);
    bus.SetMSG(true);
    EXPECT_EQ(BusPhase::MSG_OUT, bus.GetPhase());

    bus.SetIO(false);
    bus.SetCD(true);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::COMMAND, bus.GetPhase());

    bus.SetIO(false);
    bus.SetCD(false);
    bus.SetMSG(false);
    EXPECT_EQ(BusPhase::DATA_OUT, bus.GetPhase());
}

TEST(InProcessBusTest, Reset)
{
    InProcessBus bus("", false);

    bus.SetSignal(PIN_BSY, true);
    EXPECT_TRUE(bus.GetSignal(PIN_BSY));
    bus.Reset();
    EXPECT_FALSE(bus.GetSignal(PIN_BSY));
}

TEST(InProcessBusTest, SetGetSignal)
{
    InProcessBus bus("", false);

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.GetSignal(PIN_REQ));
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.GetSignal(PIN_REQ));
}

TEST(InProcessBusTest, WaitSignal)
{
    InProcessBus bus("", false);

    bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(bus.WaitSignal(PIN_ACK, true));

    bus.SetSignal(PIN_ACK, false);
    bus.SetSignal(PIN_RST, true);
    EXPECT_FALSE(bus.WaitSignal(PIN_ACK, true));
}

TEST(InProcessBusTest, WaitForSelection)
{
    InProcessBus bus("", false);

    EXPECT_TRUE(bus.WaitForSelection());
}
