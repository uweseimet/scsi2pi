//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "devices/mode_page_util.h"

using namespace mode_page_util;

TEST(ModePageUtilTest, ModeSelect6)
{
    const int LENGTH = 26;

    vector<int> cdb(6);
    vector<uint8_t> buf(LENGTH);

    // PF (vendor-specific parameter format) must not fail but be ignored
    cdb[1] = 0x00;
    ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, LENGTH, 0);

    // PF (standard parameter format)
    cdb[1] = 0x10;

    // A length of 0 is valid, the page data are optional
    ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, 0, 0);

    // Page 0
    buf[4] = 0x00;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, LENGTH, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Unsupported page 0 was not rejected";

    // Page 1 (Read-write error recovery page)
    buf[4] = 0x01;
    // Page length
    buf[5] = 0x0a;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, 12, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, 16, 512);

    // Page 7 (Verify error recovery page)
    buf[4] = 0x07;
    // Page length
    buf[5] = 0x06;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, 6, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, 10, 512);

    // Page 3 (Format device page)
    buf[4] = 0x03;
    // Page length
    buf[5] = 0x16;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, LENGTH, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Requested sector size does not match current sector size";

    // Match the requested to the current sector size
    buf[16] = 0x02;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, LENGTH - 10, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";

    ModeSelect(scsi_command::cmd_mode_select6, cdb, buf, LENGTH, 512);
}

TEST(ModePageUtilTest, ModeSelect10)
{
    const int LENGTH = 30;

    vector<int> cdb(10);
    vector<uint8_t> buf(LENGTH);

    // PF (vendor-specific parameter format) must not fail but be ignored
    cdb[1] = 0x00;
    ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, LENGTH, 0);

    // PF (standard parameter format)
    cdb[1] = 0x10;

    // A length of 0 is valid, the page data are optional
    ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, 0, 0);

    // Page 0
    buf[8] = 0x00;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, LENGTH, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
    << "Unsupported page 0 was not rejected";

    // Page 1 (Read-write error recovery page)
    buf[8] = 0x01;
    // Page length
    buf[9] = 0x0a;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, 16, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, 20, 512);

    // Page 7 (Verify error recovery page)
    buf[8] = 0x07;
    // Page length
    buf[9] = 0x06;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, 10, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";
    ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, 14, 512);

    // Page 3 (Format device page)
    buf[8] = 0x03;
    // Page length
    buf[9] = 0x16;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, LENGTH, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
        << "Requested sector size does not match current sector size";

    // Match the requested to the current sector size
    buf[20] = 0x02;
    EXPECT_THAT([&] {ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, LENGTH - 10, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
    << "Not enough command parameters";

    ModeSelect(scsi_command::cmd_mode_select10, cdb, buf, LENGTH, 512);
}

TEST(ModePageUtilTest, EvaluateBlockDescriptors)
{
    vector<uint8_t> buf(8);

    EXPECT_THAT([&] {EvaluateBlockDescriptors(scsi_command::cmd_mode_select6, buf, 0, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));

    EXPECT_THAT([&] {EvaluateBlockDescriptors(scsi_command::cmd_mode_select10, buf, 0, 512);},
        Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));
}

TEST(ModePageUtilTest, HandleSectorSizeChange)
{
    vector<uint8_t> buf = { 0x02, 0x00 };

    HandleSectorSizeChange(buf, 0, 512);

    buf[0] = 0x04;
    EXPECT_THAT([&] {HandleSectorSizeChange(buf, 0, 512);},
         Throws<scsi_exception>(AllOf(
                 Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                 Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));

    buf[0] = 0x02;
    buf[1] = 0x01;
    EXPECT_THAT([&] {HandleSectorSizeChange(buf, 0, 512);},
         Throws<scsi_exception>(AllOf(
                 Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                 Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))));
}

TEST(ModePageUtilTest, EnrichFormatPage)
{
    const int SECTOR_SIZE = 512;

    map<int, vector<byte>> pages;
    vector<byte> format_page(24);
    pages[3] = format_page;

    EnrichFormatPage(pages, false, SECTOR_SIZE);
    format_page = pages[3];
    EXPECT_EQ(byte { 0 }, format_page[12]);
    EXPECT_EQ(byte { 0 }, format_page[13]);

    EnrichFormatPage(pages, true, SECTOR_SIZE);
    format_page = pages[3];
    EXPECT_EQ(byte { SECTOR_SIZE >> 8 }, format_page[12]);
    EXPECT_EQ(byte { 0 }, format_page[13]);
}

TEST(ModePageUtilTest, AddAppleVendorModePage)
{
    map<int, vector<byte>> pages;
    vector<byte> vendor_page(30);
    pages[48] = vendor_page;

    AddAppleVendorModePage(pages, true);
    vendor_page = pages[48];
    EXPECT_EQ(byte { 0 }, vendor_page[2]);

    AddAppleVendorModePage(pages, false);
    vendor_page = pages[48];
    EXPECT_STREQ("APPLE COMPUTER, INC   ", (const char* )&vendor_page[2]);
}
