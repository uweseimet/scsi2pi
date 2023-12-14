//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "base/device_factory.h"

TEST(DeviceFactoryTest, GetTypeForFile)
{
    DeviceFactory device_factory;

    EXPECT_EQ(device_factory.GetTypeForFile("test.hd1"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hds"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.HDS"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hda"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hdr"), SCRM);
    EXPECT_EQ(device_factory.GetTypeForFile("test.mos"), SCMO);
    EXPECT_EQ(device_factory.GetTypeForFile("test.iso"), SCCD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.is1"), SCCD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.suffix.iso"), SCCD);
    EXPECT_EQ(device_factory.GetTypeForFile("daynaport"), SCDP);
    EXPECT_EQ(device_factory.GetTypeForFile("printer"), SCLP);
    EXPECT_EQ(device_factory.GetTypeForFile("services"), SCHS);
    EXPECT_EQ(device_factory.GetTypeForFile("unknown"), UNDEFINED);
    EXPECT_EQ(device_factory.GetTypeForFile("test.iso.suffix"), UNDEFINED);
}

TEST(DeviceFactoryTest, GetExtensionMapping)
{
    DeviceFactory device_factory;

    auto mapping = device_factory.GetExtensionMapping();
    EXPECT_EQ(7, mapping.size());
    EXPECT_EQ(SCHD, mapping["hd1"]);
    EXPECT_EQ(SCHD, mapping["hds"]);
    EXPECT_EQ(SCHD, mapping["hda"]);
    EXPECT_EQ(SCRM, mapping["hdr"]);
    EXPECT_EQ(SCMO, mapping["mos"]);
    EXPECT_EQ(SCCD, mapping["iso"]);
    EXPECT_EQ(SCCD, mapping["is1"]);
}

TEST(DeviceFactoryTest, UnknownDeviceType)
{
    DeviceFactory device_factory;

    auto device1 = device_factory.CreateDevice(UNDEFINED, 0, "test");
    EXPECT_EQ(nullptr, device1);
}
