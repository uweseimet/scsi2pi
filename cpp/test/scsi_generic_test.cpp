//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "test_shared.h"
#include "devices/scsi_generic.h"

TEST(ScsiGenericTest, Device_Defaults)
{
    ScsiGeneric device(0, "");

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

    const auto& [vendor, product, revision] = device.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("", product);
    EXPECT_EQ(testing::TestShared::GetVersion(), revision);
}

TEST(ScsiGenericTest, GetDevice)
{
    ScsiGeneric device(0, "device");

    EXPECT_EQ("device", device.GetDevice());
}

TEST(ScsiGenericTest, SetProductData)
{
    ScsiGeneric device(0, "");

    EXPECT_TRUE(device.SetProductData( {"", "", ""} ).empty());
    EXPECT_FALSE(device.SetProductData( {"1", "", ""} ).empty());
    EXPECT_FALSE(device.SetProductData( {"", "2", ""} ).empty());
    EXPECT_FALSE(device.SetProductData( {"", "", "3"} ).empty());
}

TEST(ScsiGenericTest, SetUp)
{
    ScsiGeneric device1(0, "");
    device1.GetLogger();
    EXPECT_FALSE(device1.SetUp());

    ScsiGeneric device2(0, "/dev/null");
    device2.GetLogger();
    EXPECT_FALSE(device2.SetUp());

    ScsiGeneric device3(0, "/dev/sg0123456789");
    device3.GetLogger();
    EXPECT_FALSE(device3.SetUp());
}

TEST(ScsiGenericTest, Dispatch)
{
    ScsiGeneric device(0, "");

    EXPECT_THROW(device.Dispatch(static_cast<scsi_command>(0x1f)), scsi_exception);
}
