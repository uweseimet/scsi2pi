//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "base/device_factory.h"
#include "devices/daynaport.h"
#include "devices/host_services.h"
#include "devices/optical_memory.h"
#include "devices/printer.h"
#include "devices/sasi_hd.h"
#include "devices/scsi_cd.h"
#include "devices/scsi_hd.h"

TEST(DeviceFactoryTest, CreateDevice)
{
    const DeviceFactory &device_factory = DeviceFactory::Instance();

    EXPECT_EQ(SCHD, device_factory.CreateDevice(SCHD, 0, "")->GetType());
    EXPECT_EQ(SCRM, device_factory.CreateDevice(SCRM, 0, "")->GetType());
    EXPECT_EQ(SCMO, device_factory.CreateDevice(SCMO, 0, "")->GetType());
    EXPECT_EQ(SCCD, device_factory.CreateDevice(SCCD, 0, "")->GetType());
    EXPECT_EQ(SCDP, device_factory.CreateDevice(SCDP, 0, "")->GetType());
    EXPECT_EQ(SCLP, device_factory.CreateDevice(SCLP, 0, "")->GetType());
    EXPECT_EQ(SCHS, device_factory.CreateDevice(SCHS, 0, "")->GetType());
    EXPECT_EQ(SCTP, device_factory.CreateDevice(SCTP, 0, "")->GetType());
    EXPECT_EQ(SAHD, device_factory.CreateDevice(SAHD, 0, "")->GetType());

    EXPECT_EQ(nullptr, device_factory.CreateDevice(UNDEFINED, 0, ""));
}

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
    const auto &mapping = DeviceFactory::Instance().GetExtensionMapping();

    EXPECT_EQ(10U, mapping.size());
    EXPECT_EQ(SCHD, mapping.at("hd1"));
    EXPECT_EQ(SCHD, mapping.at("hds"));
    EXPECT_EQ(SCHD, mapping.at("hda"));
    EXPECT_EQ(SCRM, mapping.at("hdr"));
    EXPECT_EQ(SCMO, mapping.at("mos"));
    EXPECT_EQ(SCCD, mapping.at("iso"));
    EXPECT_EQ(SCCD, mapping.at("cdr"));
    EXPECT_EQ(SCCD, mapping.at("toast"));
    EXPECT_EQ(SCCD, mapping.at("is1"));
    EXPECT_EQ(SCTP, mapping.at("tar"));
}

TEST(DeviceFactoryTest, AddExtensionMapping)
{
    DeviceFactory &device_factory = DeviceFactory::Instance();

    EXPECT_FALSE(device_factory.AddExtensionMapping("iso", SCHS));
    auto mapping = device_factory.GetExtensionMapping();
    EXPECT_EQ(10U, mapping.size());

    EXPECT_TRUE(device_factory.AddExtensionMapping("ext", SCCD));
    mapping = device_factory.GetExtensionMapping();
    EXPECT_EQ(11U, mapping.size());
    EXPECT_EQ(SCCD, mapping["ext"]);
}
