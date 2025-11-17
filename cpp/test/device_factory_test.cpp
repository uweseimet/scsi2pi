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
    const DeviceFactory &factory = DeviceFactory::GetInstance();

    EXPECT_EQ(SCHD, factory.CreateDevice(SCHD, 0, "")->GetType());
    EXPECT_EQ(SCRM, factory.CreateDevice(SCRM, 0, "")->GetType());
    EXPECT_EQ(SCMO, factory.CreateDevice(SCMO, 0, "")->GetType());
    EXPECT_EQ(SCCD, factory.CreateDevice(SCCD, 0, "")->GetType());
    EXPECT_EQ(SCDP, factory.CreateDevice(SCDP, 0, "")->GetType());
    EXPECT_EQ(SCLP, factory.CreateDevice(SCLP, 0, "")->GetType());
    EXPECT_EQ(SCHS, factory.CreateDevice(SCHS, 0, "")->GetType());
#ifdef BUILD_SCSG
    EXPECT_EQ(SCSG, factory.CreateDevice(SCSG, 0, "")->GetType());
#endif
    EXPECT_EQ(SCTP, factory.CreateDevice(SCTP, 0, "")->GetType());
    EXPECT_EQ(SAHD, factory.CreateDevice(SAHD, 0, "")->GetType());

    EXPECT_EQ(nullptr, factory.CreateDevice(UNDEFINED, 0, ""));
}

TEST(DeviceFactoryTest, GetTypeForFile)
{
    const DeviceFactory &factory = DeviceFactory::GetInstance();

    EXPECT_EQ(factory.GetTypeForFile("test.hd1"), SCHD);
    EXPECT_EQ(factory.GetTypeForFile("test.hds"), SCHD);
    EXPECT_EQ(factory.GetTypeForFile("test.HDS"), SCHD);
    EXPECT_EQ(factory.GetTypeForFile("test.hda"), SCHD);
    EXPECT_EQ(factory.GetTypeForFile("test.hdr"), SCRM);
    EXPECT_EQ(factory.GetTypeForFile("test.mos"), SCMO);
    EXPECT_EQ(factory.GetTypeForFile("test.iso"), SCCD);
    EXPECT_EQ(factory.GetTypeForFile("test.cdr"), SCCD);
    EXPECT_EQ(factory.GetTypeForFile("test.toast"), SCCD);
    EXPECT_EQ(factory.GetTypeForFile("test.is1"), SCCD);
    EXPECT_EQ(factory.GetTypeForFile("test.suffix.iso"), SCCD);
    EXPECT_EQ(factory.GetTypeForFile("daynaport"), SCDP);
    EXPECT_EQ(factory.GetTypeForFile("printer"), SCLP);
    EXPECT_EQ(factory.GetTypeForFile("services"), SCHS);
#ifdef BUILD_SCSG
    EXPECT_EQ(factory.GetTypeForFile("/dev/sg0"), SCSG);
#endif
    EXPECT_EQ(factory.GetTypeForFile("unknown"), UNDEFINED);
    EXPECT_EQ(factory.GetTypeForFile("test.iso.suffix"), UNDEFINED);
}

TEST(DeviceFactoryTest, GetExtensionMapping)
{
    const auto &mapping = DeviceFactory::GetInstance().GetExtensionMapping();

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
    EXPECT_EQ(SCTP, mapping.at("tap"));
}

TEST(DeviceFactoryTest, AddExtensionMapping)
{
    DeviceFactory &factory = DeviceFactory::GetInstance();

    EXPECT_FALSE(factory.AddExtensionMapping("iso", SCHS));
    auto mapping = factory.GetExtensionMapping();
    EXPECT_EQ(11U, mapping.size());

    EXPECT_TRUE(factory.AddExtensionMapping("ext", SCCD));
    mapping = factory.GetExtensionMapping();
    EXPECT_EQ(12U, mapping.size());
    EXPECT_EQ(SCCD, mapping["ext"]);
}
