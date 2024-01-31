//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "base/device_factory.h"

TEST(DeviceFactoryTest, GetTypeForFile)
{
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    EXPECT_EQ(device_factory.GetTypeForFile("test.hd1"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hds"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.HDS"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hda"), SCHD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.hdr"), SCRM);
    EXPECT_EQ(device_factory.GetTypeForFile("test.mos"), SCMO);
    EXPECT_EQ(device_factory.GetTypeForFile("test.iso"), SCCD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.cdr"), SCCD);
    EXPECT_EQ(device_factory.GetTypeForFile("test.toast"), SCCD);
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
    auto mapping = DeviceFactory::Instance().GetExtensionMapping();
    EXPECT_EQ(9U, mapping.size());
    EXPECT_EQ(SCHD, mapping["hd1"]);
    EXPECT_EQ(SCHD, mapping["hds"]);
    EXPECT_EQ(SCHD, mapping["hda"]);
    EXPECT_EQ(SCRM, mapping["hdr"]);
    EXPECT_EQ(SCMO, mapping["mos"]);
    EXPECT_EQ(SCCD, mapping["iso"]);
    EXPECT_EQ(SCCD, mapping["cdr"]);
    EXPECT_EQ(SCCD, mapping["toast"]);
    EXPECT_EQ(SCCD, mapping["is1"]);
}

TEST(DeviceFactoryTest, UnknownDeviceType)
{
    auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "test");
    EXPECT_EQ(nullptr, device);
}
