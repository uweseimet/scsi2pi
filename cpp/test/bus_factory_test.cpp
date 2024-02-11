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
    auto target = BusFactory::Instance().CreateBus(true, true);
    EXPECT_NE(nullptr, target);
    // Avoid a delay by signalling the initiator that the target is ready
    target->CleanUp();
    EXPECT_NE(nullptr, BusFactory::Instance().CreateBus(false, true));
}
