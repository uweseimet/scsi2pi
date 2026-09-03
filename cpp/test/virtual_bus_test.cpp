//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "../buses/virtual_bus.h"

TEST(VirtualBusTest, BSY)
{
    VirtualBus bus("", true);

    bus.SetBSY(true);
    EXPECT_TRUE(bus.GetBSY());
    bus.SetBSY(false);
    EXPECT_FALSE(bus.GetBSY());
}

TEST(VirtualBusTest, SEL)
{
    VirtualBus bus("", true);

    bus.SetSEL(true);
    EXPECT_TRUE(bus.GetSEL());
    bus.SetSEL(false);
    EXPECT_FALSE(bus.GetSEL());
}

TEST(VirtualBusTest, ATN)
{
    VirtualBus bus("", true);

    bus.SetATN(true);
    EXPECT_TRUE(bus.GetATN());
    bus.SetATN(false);
    EXPECT_FALSE(bus.GetATN());
}

TEST(VirtualBusTest, ACK)
{
    VirtualBus bus("", true);

    bus.SetACK(true);
    EXPECT_TRUE(bus.GetACK());
    bus.SetACK(false);
    EXPECT_FALSE(bus.GetACK());
}

TEST(VirtualBusTest, REQ)
{
    VirtualBus bus("", true);

    bus.SetREQ(true);
    EXPECT_TRUE(bus.GetREQ());
    bus.SetREQ(false);
    EXPECT_FALSE(bus.GetREQ());
}

TEST(VirtualBusTest, RST)
{
    VirtualBus bus("", false);

    bus.SetRST(true);
    EXPECT_TRUE(bus.GetRST());
    bus.SetRST(false);
    EXPECT_FALSE(bus.GetRST());
}

TEST(VirtualBusTest, MSG)
{
    VirtualBus bus("", false);

    bus.SetMSG(true);
    EXPECT_TRUE(bus.GetMSG());
    bus.SetMSG(false);
    EXPECT_FALSE(bus.GetMSG());
}

TEST(VirtualBusTest, CD)
{
    VirtualBus bus("", false);

    bus.SetCD(true);
    EXPECT_TRUE(bus.GetCD());
    bus.SetCD(false);
    EXPECT_FALSE(bus.GetCD());
}

TEST(VirtualBusTest, IO)
{
    VirtualBus bus("", false);

    bus.SetIO(true);
    EXPECT_TRUE(bus.GetIO());
    bus.SetIO(false);
    EXPECT_FALSE(bus.GetIO());
}

TEST(VirtualBusTest, DAT)
{
    VirtualBus bus("", false);

    bus.SetDAT(0xae);
    EXPECT_EQ(0xae, bus.GetDAT());
    bus.SetDAT(0x21);
    EXPECT_EQ(0x21, bus.GetDAT());
}

TEST(VirtualBusTest, Acquire)
{
    VirtualBus bus("", false);

    bus.SetDAT(0x12);
    bus.Acquire();
    EXPECT_EQ(0x12U, bus.GetDAT());
}

TEST(VirtualBusTest, BusPhases)
{
    VirtualBus bus("", false);

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

TEST(VirtualBusTest, Init)
{
    VirtualBus bus("", false);

    bus.SetSignals(0x12345678U);
    EXPECT_TRUE(bus.Init(false));
    EXPECT_EQ(0xffffffffU, bus.GetSignals());

    bus.SetSignals(0x12345678U);
    EXPECT_TRUE(bus.Init(true));
    EXPECT_EQ(0xffffffffU, bus.GetSignals());
}

TEST(VirtualBusTest, Reset)
{
    VirtualBus bus("", false);

    bus.SetSignal(PIN_BSY, true);
    EXPECT_TRUE(bus.GetSignal(PIN_BSY_MASK));
    bus.Reset();
    EXPECT_FALSE(bus.GetSignal(PIN_BSY_MASK));
}

TEST(VirtualBusTest, SetGetSignal)
{
    VirtualBus bus("", false);

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.GetSignal(PIN_REQ_MASK));
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.GetSignal(PIN_REQ_MASK));
}

TEST(VirtualBusTest, WaitHandshakeACK)
{
    VirtualBus bus("", false);

    bus.SetSignal(PIN_ACK, true);
    EXPECT_TRUE(bus.WaitHandShake(PIN_ACK_MASK, true));

    bus.SetSignal(PIN_ACK, false);
    EXPECT_TRUE(bus.WaitHandShake(PIN_ACK_MASK, false));

    bus.SetSignal(PIN_RST, true);
    bus.SetSignal(PIN_ACK, false);
    EXPECT_FALSE(bus.WaitHandShake(PIN_ACK_MASK, true));
}

TEST(VirtualBusTest, WaitHandshakeREQ)
{
    VirtualBus bus("", false);

    bus.SetSignal(PIN_REQ, true);
    EXPECT_TRUE(bus.WaitHandShake(PIN_REQ_MASK, true));

    bus.SetSignal(PIN_REQ, false);
    EXPECT_TRUE(bus.WaitHandShake(PIN_REQ_MASK, false));

    bus.SetSignal(PIN_RST, true);
    bus.SetSignal(PIN_REQ, false);
    EXPECT_FALSE(bus.WaitHandShake(PIN_REQ_MASK, true));
}

TEST(VirtualBusTest, IsRaspberryPi)
{
    VirtualBus bus("", false);

    EXPECT_FALSE(bus.IsRaspberryPi());
}
