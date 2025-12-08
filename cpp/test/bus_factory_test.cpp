//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus_factory.h"

using namespace bus_factory;

TEST(BusFactoryTest, CreateBus)
{
    auto bus = CreateBus(true, true, "", false);
    EXPECT_NE(nullptr, bus);
    // Avoid a delay by signalling the initiator that the target is ready
    bus->Ready();
    EXPECT_NE(nullptr, CreateBus(false, true, "", false));
}
