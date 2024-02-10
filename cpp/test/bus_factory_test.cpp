//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus_factory.h"

TEST(BusFactoryTest, CreateBus)
{
    BusFactory bus_factory;

    EXPECT_NE(nullptr, bus_factory.CreateBus(true, true));
    EXPECT_NE(nullptr, bus_factory.CreateBus(false, true));
}
