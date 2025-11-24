//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#ifdef __linux__

#include <gtest/gtest.h>
#include "pi/rpi_bus.h"

TEST(RpiBusTest, GetPiType)
{
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType("Raspberry Pi 1"));
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType("Raspberry Pi Zero"));
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType("Raspberry Pi Model B Plus"));
    EXPECT_EQ(RpiBus::PiType::PI_2, RpiBus::GetPiType("Raspberry Pi 2"));
    EXPECT_EQ(RpiBus::PiType::PI_3, RpiBus::GetPiType("Raspberry Pi 3"));
    EXPECT_EQ(RpiBus::PiType::PI_3, RpiBus::GetPiType("Raspberry Pi Zero 2"));
    EXPECT_EQ(RpiBus::PiType::PI_4, RpiBus::GetPiType("Raspberry Pi 4"));
    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType("Raspberry Pi 5"));
    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType(""));
    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType("abc"));
}

#endif
