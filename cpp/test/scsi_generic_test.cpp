//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#ifdef __linux__

#include <gtest/gtest.h>
#include "devices/scsi_generic.h"

TEST(ScsiGenericTest, Device_Defaults)
{
    ScsiGeneric device(0);

    EXPECT_EQ(SCSG, device.GetType());
    EXPECT_FALSE(device.SupportsImageFile());
    EXPECT_TRUE(device.SupportsParams());
    EXPECT_FALSE(device.IsProtectable());
    EXPECT_FALSE(device.IsProtected());
    EXPECT_FALSE(device.IsReadOnly());
    EXPECT_FALSE(device.IsRemovable());
    EXPECT_FALSE(device.IsRemoved());
    EXPECT_FALSE(device.IsLocked());
    EXPECT_FALSE(device.IsStoppable());
    EXPECT_FALSE(device.IsStopped());
}

TEST(ScsiGenericTest, GetDefaultParams)
{
    ScsiGeneric device(0);

    const auto &params = device.GetDefaultParams();
    EXPECT_EQ(1U, params.size());
    EXPECT_EQ("", params.at("device"));
}

#endif
