//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

// Note that this test depends on no conflicting global properties being defined in /etc/s2p.conf

#include <gtest/gtest.h>
#include "base/property_handler.h"
#include "shared/s2p_exceptions.h"
#include "test_shared.h"

using namespace testing;

void SetUpProperties(string_view properties1, string_view properties2 = "", const property_map &cmd_properties = { })
{
    string filenames;
    auto [fd1, filename1] = OpenTempFile();
    filenames = filename1;
    (void)write(fd1, properties1.data(), properties1.size());
    close(fd1);
    if (!properties2.empty()) {
        auto [fd2, filename2] = OpenTempFile();
        filenames += ",";
        filenames += filename2;
        (void)write(fd2, properties2.data(), properties2.size());
        close(fd2);
    }
    PropertyHandler::Instance().Init(filenames, cmd_properties, true);
}

TEST(PropertyHandlerTest, Init)
{
    const string &properties1 =
        R"(key1=value1
key2=value2
device.3.params=params3
)";
    const string &properties2 =
        R"(key3=value3
)";
    const string &properties3 =
        R"(key
)";

    PropertyHandler::Instance().Init("", { }, true);
    EXPECT_THROW(PropertyHandler::Instance().Init("non_existing_file", { }, true), parser_exception);

    property_map cmd_properties;
    cmd_properties["key1"] = "value2";
    cmd_properties["device.1.params"] = "params1";
    cmd_properties["device.2:1.params"] = "params2";
    SetUpProperties(properties1, properties2, cmd_properties);
    EXPECT_EQ("value2", PropertyHandler::Instance().GetProperty("key1"));
    EXPECT_EQ("value2", PropertyHandler::Instance().GetProperty("key2"));
    EXPECT_EQ("value3", PropertyHandler::Instance().GetProperty("key3"));
    EXPECT_EQ("params1", PropertyHandler::Instance().GetProperty("device.1:0.params"));
    EXPECT_EQ("params2", PropertyHandler::Instance().GetProperty("device.2:1.params"));
    EXPECT_EQ("params3", PropertyHandler::Instance().GetProperty("device.3:0.params"));

    EXPECT_THROW(SetUpProperties(properties3), parser_exception);
}

TEST(PropertyHandlerTest, GetProperties)
{
    const string &properties =
        R"(key1=value1
key2=value2
key11=value2
)";

    SetUpProperties(properties);

    auto p = PropertyHandler::Instance().GetProperties("key2");
    EXPECT_EQ(1U, p.size());
    EXPECT_TRUE(p.contains("key2"));

    p = PropertyHandler::Instance().GetProperties("key1");
    EXPECT_EQ(2U, p.size());
    EXPECT_TRUE(p.contains("key1"));
    EXPECT_TRUE(p.contains("key11"));
}

TEST(PropertyHandlerTest, GetProperty)
{
    const string &properties =
        R"(key1=value1
key2=value2
)";

    SetUpProperties(properties);

    EXPECT_TRUE(PropertyHandler::Instance().GetProperty("key").empty());
    EXPECT_TRUE(PropertyHandler::Instance().GetProperty("key3").empty());
    EXPECT_EQ("value1", PropertyHandler::Instance().GetProperty("key1"));
    EXPECT_EQ("value2", PropertyHandler::Instance().GetProperty("key2"));

    EXPECT_EQ("default_value", PropertyHandler::Instance().GetProperty("key", "default_value"));
}

TEST(PropertyHandlerTest, GetCustomModePages)
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


    SetUpProperties(properties1);

    auto mode_pages = PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT");
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
    mode_pages = PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT");
    EXPECT_EQ(1UL, mode_pages.size());
    value = mode_pages.at(1);
    EXPECT_EQ(4UL, value.size());
    EXPECT_EQ(byte { 0x81 }, value[0]);
    EXPECT_EQ(byte { 0x02 }, value[1]);
    EXPECT_EQ(byte { 0xef }, value[2]);
    EXPECT_EQ(byte { 0xff }, value[3]);

    SetUpProperties(properties_codes_inconsistent);
    EXPECT_TRUE(PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_length_wrong);
    EXPECT_TRUE(PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_code_invalid);
    EXPECT_TRUE(PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT").empty());

    SetUpProperties(properties_format_invalid);
    EXPECT_TRUE(PropertyHandler::Instance().GetCustomModePages("VENDOR", "PRODUCT").empty());
}
