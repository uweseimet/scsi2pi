//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus_factory.h"

TEST(BusFactoryTest, CreateBus)
{
    EXPECT_NE(nullptr, BusFactory::GetInstance().CreateBus(true, ""));
    EXPECT_NE(nullptr, BusFactory::GetInstance().CreateBus(false, ""));
}
