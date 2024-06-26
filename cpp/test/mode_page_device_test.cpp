//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/mode_page_device.h"
#include "shared/s2p_exceptions.h"

TEST(ModePageDeviceTest, AddModePages)
{
    vector<uint8_t> buf(512);
    MockModePageDevice device;

    // Page 0
    vector<int> cdb = CreateCdb(scsi_command::cmd_mode_select6);
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, 255);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Data were returned for non-existing mode page 0";

    // All pages, non changeable
    cdb = CreateCdb(scsi_command::cmd_mode_select6, "00:3f");
    EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
    EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, -1);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb)))) << "Maximum size was ignored";

    // All pages, changeable
    cdb = CreateCdb(scsi_command::cmd_mode_select6, "00:7f");
    EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
    EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, -1);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb)))) << "Maximum size was ignored";
}

TEST(ModePageDeviceTest, AddVendorPages)
{
    map<int, vector<byte>> pages;
    MockModePageDevice device;

    device.AddVendorPages(pages, 0x3f, false);
    EXPECT_TRUE(pages.empty()) << "Unexpected default vendor mode page";
    device.AddVendorPages(pages, 0x3f, true);
    EXPECT_TRUE(pages.empty()) << "Unexpected default vendor mode page";
}

TEST(ModePageDeviceTest, ModeSense)
{
    MockAbstractController controller(0);
    const auto device = make_shared<NiceMock<MockModePageDevice>>();
    EXPECT_TRUE(device->Init( { }));

    controller.AddDevice(device);

    EXPECT_CALL(controller, DataIn());
    EXPECT_NO_THROW(device->Dispatch(scsi_command::cmd_mode_sense6));

    EXPECT_CALL(controller, DataIn());
    EXPECT_NO_THROW(device->Dispatch(scsi_command::cmd_mode_sense10));
}

TEST(ModePageDeviceTest, ModeSelect)
{
    MockModePageDevice device;

    EXPECT_THROW(device.ModeSelect( { }, { }, 0), scsi_exception);
}
