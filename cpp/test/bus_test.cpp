//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(BusTest, GetPhaseName)
{
    EXPECT_EQ("BUS FREE", Bus::GetPhaseName(BusPhase::BUS_FREE));
    EXPECT_EQ("ARBITRATION", Bus::GetPhaseName(BusPhase::ARBITRATION));
    EXPECT_EQ("SELECTION", Bus::GetPhaseName(BusPhase::SELECTION));
    EXPECT_EQ("RESELECTION", Bus::GetPhaseName(BusPhase::RESELECTION));
    EXPECT_EQ("COMMAND", Bus::GetPhaseName(BusPhase::COMMAND));
    EXPECT_EQ("DATA IN", Bus::GetPhaseName(BusPhase::DATA_IN));
    EXPECT_EQ("DATA OUT", Bus::GetPhaseName(BusPhase::DATA_OUT));
    EXPECT_EQ("STATUS", Bus::GetPhaseName(BusPhase::STATUS));
    EXPECT_EQ("MESSAGE IN", Bus::GetPhaseName(BusPhase::MSG_IN));
    EXPECT_EQ("MESSAGE OUT", Bus::GetPhaseName(BusPhase::MSG_OUT));
    EXPECT_EQ("????", Bus::GetPhaseName(BusPhase::RESERVED));
}

TEST(BusTest, Reset)
{
    MockBus bus;

    bus.SetSignals(0x12345678U);
    EXPECT_EQ(0x12345678U, bus.GetSignals());
    EXPECT_CALL(bus, SetDir);
    bus.Reset();
    EXPECT_EQ(0xffffffffU, bus.GetSignals());
}

TEST(BusTest, Signals)
{
    MockBus bus;

    bus.SetSignals(0x12345678U);
    EXPECT_EQ(0x12345678U, bus.GetSignals());
    bus.SetSignals(0x87654321U);
    EXPECT_EQ(0x87654321U, bus.GetSignals());
}

TEST(BusTest, GetDAT)
{
    MockBus bus;

    bus.SetSignals(~0b00000000000000111111110000000000);
    EXPECT_CALL(bus, Acquire);
    EXPECT_EQ(0b11111111, bus.GetDAT());
}

TEST(BusTest, IsTarget)
{
    MockBus bus;

    bus.Init(true);
    EXPECT_TRUE(bus.IsTarget());
    bus.Init(false);
    EXPECT_FALSE(bus.IsTarget());
}
