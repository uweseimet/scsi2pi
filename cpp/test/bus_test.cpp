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
