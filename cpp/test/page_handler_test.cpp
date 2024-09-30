//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

// Note that this test depends on no conflicting global properties being defined in /etc/s2p.conf

#include "mocks.h"
#include "shared/s2p_exceptions.h"
#include "devices/page_handler.h"
#include "test_shared.h"

using namespace testing;

TEST(PageHandlerTest, GetCustomModePages)
{
    const string &properties1 =
        R"(mode_page.0.VENDOR=0010020304ff
mode_page.2.VENDOR:PRODUCT=02:01:B0
mode_page.3.VENDOR:PRODUCT=

mode_page.1._:PRODUCT2=
#mode_page.4.VENDOR=040101
)";

    const string &properties_savable =
        R"(mode_page.1.VENDOR:PRODUCT=81:02:ef:ff
)";

    const string &properties_codes_inconsistent =
        R"(mode_page.1.VENDOR:PRODUCT=03:02:ef:ff
)";

    const string &properties_length_wrong =
        R"(mode_page.1.VENDOR:PRODUCT=01:03:fe:ff
)";

    const string &properties_code_invalid =
        R"(mode_page.63.VENDOR:PRODUCT=3f:01:ff
)";

    const string &properties_format_invalid =
        R"(mode_page.2.VENDOR:PRODUCT=02:1:ff
)";

    MockPrimaryDevice device(0);
    PageHandler page_handler(device, false, false);

    SetUpProperties(properties1);

    auto mode_pages = page_handler.GetCustomModePages("VENDOR", "PRODUCT");
    EXPECT_EQ(3UL, mode_pages.size());
    auto value = mode_pages.at(0);
    EXPECT_EQ(6UL, value.size());
    EXPECT_EQ(byte { 0x00 }, value[0]);
    EXPECT_EQ(byte { 0x10 }, value[1]);
    EXPECT_EQ(byte { 0x02 }, value[2]);
    EXPECT_EQ(byte { 0x03 }, value[3]);
    EXPECT_EQ(byte { 0x04 }, value[4]);
    EXPECT_EQ(byte { 0xff }, value[5]);
    value = mode_pages.at(2);
    EXPECT_EQ(3UL, value.size());
    EXPECT_EQ(byte { 0x02 }, value[0]);
    EXPECT_EQ(byte { 0x01 }, value[1]);
    EXPECT_EQ(byte { 0xb0 }, value[2]);
    value = mode_pages.at(3);
    EXPECT_TRUE(value.empty());

    SetUpProperties(properties_savable);
    mode_pages = page_handler.GetCustomModePages("VENDOR", "PRODUCT");
    EXPECT_EQ(1UL, mode_pages.size());
    value = mode_pages.at(1);
    EXPECT_EQ(4UL, value.size());
    EXPECT_EQ(byte { 0x81 }, value[0]);
    EXPECT_EQ(byte { 0x02 }, value[1]);
    EXPECT_EQ(byte { 0xef }, value[2]);
    EXPECT_EQ(byte { 0xff }, value[3]);

    SetUpProperties(properties_codes_inconsistent);
    EXPECT_TRUE(page_handler.GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_length_wrong);
    EXPECT_TRUE(page_handler.GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_code_invalid);
    EXPECT_TRUE(page_handler.GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_format_invalid);
    EXPECT_TRUE(page_handler.GetCustomModePages("VENDOR", "PRODUCT").empty());
}
