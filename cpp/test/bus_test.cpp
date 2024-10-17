//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus.h"

TEST(BusTest, GetPhaseName)
{
    EXPECT_EQ("BUS FREE", Bus::GetPhaseName(bus_phase::busfree));
    EXPECT_EQ("ARBITRATION", Bus::GetPhaseName(bus_phase::arbitration));
    EXPECT_EQ("SELECTION", Bus::GetPhaseName(bus_phase::selection));
    EXPECT_EQ("RESELECTION", Bus::GetPhaseName(bus_phase::reselection));
    EXPECT_EQ("COMMAND", Bus::GetPhaseName(bus_phase::command));
    EXPECT_EQ("DATA IN", Bus::GetPhaseName(bus_phase::datain));
    EXPECT_EQ("DATA OUT", Bus::GetPhaseName(bus_phase::dataout));
    EXPECT_EQ("STATUS", Bus::GetPhaseName(bus_phase::status));
    EXPECT_EQ("MESSAGE IN", Bus::GetPhaseName(bus_phase::msgin));
    EXPECT_EQ("MESSAGE OUT", Bus::GetPhaseName(bus_phase::msgout));
    EXPECT_EQ("???", Bus::GetPhaseName(bus_phase::reserved));
}
