//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2025-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/rpi_bus.h"

TEST(GpioBusTest, IsRaspberryPi)
{
    RpiBus bus(RpiBus::PiType::PI_1, { });

    EXPECT_TRUE(bus.IsRaspberryPi());
}
