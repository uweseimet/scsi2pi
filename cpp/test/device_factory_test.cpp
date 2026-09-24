
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "devices/daynaport.h"
#include "devices/device_factory.h"
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
    EXPECT_EQ(SCLP, factory.CreateDevice(SCLP, 0, "")->GetType());
    EXPECT_EQ(SCHS, factory.CreateDevice(SCHS, 0, "")->GetType());
#if __has_include(<scsi/sg.h>)
    EXPECT_EQ(SCDP, factory.CreateDevice(SCDP, 0, "")->GetType());
    EXPECT_EQ(SCSG, factory.CreateDevice(SCSG, 0, "")->GetType());
#endif
    EXPECT_EQ(SCTP, factory.CreateDevice(SCTP, 0, "")->GetType());
    EXPECT_EQ(SAHD, factory.CreateDevice(SAHD, 0, "")->GetType());

    EXPECT_EQ(nullptr, factory.CreateDevice(UNDEFINED, 0, ""));
}

TEST(DeviceFactoryTest, GetTypeForFile)
{
    const DeviceFactory &factory = DeviceFactory::GetInstance();

    EXPECT_EQ(factory.GetTypeForFile(path("test.hdf")), SAHD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.hd1")), SCHD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.hds")), SCHD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.HDS")), SCHD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.hda")), SCHD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.hdr")), SCRM);
    EXPECT_EQ(factory.GetTypeForFile(path("test.mos")), SCMO);
    EXPECT_EQ(factory.GetTypeForFile(path("test.iso")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.cdr")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.toast")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.is1")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("test.suffix.iso")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("daynaport")), SCDP);
    EXPECT_EQ(factory.GetTypeForFile(path("printer")), SCLP);
    EXPECT_EQ(factory.GetTypeForFile(path("services")), SCHS);
    EXPECT_EQ(factory.GetTypeForFile(path("/dev/sda")), SCHD);
    EXPECT_EQ(factory.GetTypeForFile(path("/dev/sr0")), SCCD);
    EXPECT_EQ(factory.GetTypeForFile(path("/dev/sg0")), SCSG);
    EXPECT_EQ(factory.GetTypeForFile(path("unknown")), UNDEFINED);
    EXPECT_EQ(factory.GetTypeForFile(path("test.iso.suffix")), UNDEFINED);
}

TEST(DeviceFactoryTest, GetExtensionMapping)
{
    const auto &mapping = DeviceFactory::GetInstance().GetExtensionMapping();

    EXPECT_EQ(SAHD, mapping.at("hdf"));
    EXPECT_EQ(SCHD, mapping.at("hd1"));
    EXPECT_EQ(SCHD, mapping.at("hda"));
    EXPECT_EQ(SCHD, mapping.at("hds"));
    EXPECT_EQ(SCRM, mapping.at("hdr"));
    EXPECT_EQ(SCMO, mapping.at("mos"));
    EXPECT_EQ(SCCD, mapping.at("iso"));
    EXPECT_EQ(SCCD, mapping.at("cdr"));
    EXPECT_EQ(SCCD, mapping.at("toast"));
    EXPECT_EQ(SCCD, mapping.at("is1"));
    EXPECT_EQ(SCTP, mapping.at("tar"));
    EXPECT_EQ(SCTP, mapping.at("tap"));
}

TEST(DeviceFactoryTest, UpdateExtensionMapping)
{
    DeviceFactory &factory = DeviceFactory::GetInstance();

    EXPECT_FALSE(factory.UpdateExtensionMapping("iso", SCHS));
    auto mapping = factory.GetExtensionMapping();
    EXPECT_EQ(12U, mapping.size());

    EXPECT_FALSE(factory.UpdateExtensionMapping("ISO", SCHS));
    mapping = factory.GetExtensionMapping();
    EXPECT_EQ(12U, mapping.size());

    EXPECT_FALSE(factory.UpdateExtensionMapping(".iso", SCHS));
    mapping = factory.GetExtensionMapping();
    EXPECT_EQ(12U, mapping.size());

    EXPECT_FALSE(factory.UpdateExtensionMapping("", SCCD));

    EXPECT_TRUE(factory.UpdateExtensionMapping("ext", SCCD));
    mapping = factory.GetExtensionMapping();
    EXPECT_EQ(13U, mapping.size());
    EXPECT_EQ(SCCD, mapping["ext"]);

    EXPECT_TRUE(factory.UpdateExtensionMapping("ext", UNDEFINED));
    mapping = factory.GetExtensionMapping();
    EXPECT_EQ(12U, mapping.size());
}
