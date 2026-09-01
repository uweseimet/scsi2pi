//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus_factory.h"

using namespace bus_factory;

TEST(BusFactoryTest, CreateBus)
{
    EXPECT_NE(nullptr, CreateBus(true, true, false, ""));
    EXPECT_NE(nullptr, CreateBus(false, true, false, ""));
}
