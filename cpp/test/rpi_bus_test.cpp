//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <gtest/gtest.h>
#include "pi/rpi_bus.h"
#include "test_shared.h"

TEST(RpiBusTest, GetPiType)
{
    const string &filename = testing::CreateTempName();
    ofstream out(filename);

    out << "Raspberry Pi 1" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi Zero" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi Model B Plus" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_1, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi 2" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_2, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi 3" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_3, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi Zero 2" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_3, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi 4" << flush;
    EXPECT_EQ(RpiBus::PiType::PI_4, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "Raspberry Pi 5" << flush;
    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType(filename));

    out.seekp(0);
    out << "abc" << flush;
    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType(filename));

    EXPECT_EQ(RpiBus::PiType::UNKNOWN, RpiBus::GetPiType("/xyz"));
}

TEST(RpiBusTest, IsRaspberryPi)
{
    RpiBus bus;

    EXPECT_TRUE(bus.IsRaspberryPi());
}
