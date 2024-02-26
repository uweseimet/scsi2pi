//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus.h"

TEST(BusTest, GetPhaseName)
{
    EXPECT_EQ("BUS FREE", Bus::GetPhaseName(phase_t::busfree));
    EXPECT_EQ("ARBITRATION", Bus::GetPhaseName(phase_t::arbitration));
    EXPECT_EQ("SELECTION", Bus::GetPhaseName(phase_t::selection));
    EXPECT_EQ("RESELECTION", Bus::GetPhaseName(phase_t::reselection));
    EXPECT_EQ("COMMAND", Bus::GetPhaseName(phase_t::command));
    EXPECT_EQ("DATA IN", Bus::GetPhaseName(phase_t::datain));
    EXPECT_EQ("DATA OUT", Bus::GetPhaseName(phase_t::dataout));
    EXPECT_EQ("STATUS", Bus::GetPhaseName(phase_t::status));
    EXPECT_EQ("MESSAGE IN", Bus::GetPhaseName(phase_t::msgin));
    EXPECT_EQ("MESSAGE OUT", Bus::GetPhaseName(phase_t::msgout));
    EXPECT_EQ("RESERVED", Bus::GetPhaseName(phase_t::reserved));
}
