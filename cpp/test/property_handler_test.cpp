//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include <gtest/gtest.h>
#include "test/test_shared.h"
#include "base/property_handler.h"

using namespace testing;

TEST(PropertyHandlerTest, GetCustomModePages)
{
    const string properties =
        R"(mode_page.0.VENDOR=04020304ff
mode_page.2.VENDOR:PRODUCT=01B0
mode_page.3.VENDOR:PRODUCT=
mode_page.1._:PRODUCT2=
#mode_page.4.VENDOR=0101
)";

    auto property_handler = PropertyHandler::Instance();
    auto [fd, filename] = OpenTempFile();
    write(fd, properties.data(), properties.size());
    close(fd);
    property_handler.Init(filename);
    remove(filename);

    const auto &mode_pages = property_handler.GetCustomModePages("VENDOR", "PRODUCT");
    EXPECT_EQ(3, mode_pages.size());
    auto value = mode_pages.at(0);
    EXPECT_EQ(6, value.size());
    EXPECT_EQ(byte { 0x00 }, value[0]);
    EXPECT_EQ(byte { 0x04 }, value[1]);
    EXPECT_EQ(byte { 0x02 }, value[2]);
    EXPECT_EQ(byte { 0x03 }, value[3]);
    EXPECT_EQ(byte { 0x04 }, value[4]);
    EXPECT_EQ(byte { 0xff }, value[5]);
    value = mode_pages.at(2);
    EXPECT_EQ(3, value.size());
    EXPECT_EQ(byte { 0x02 }, value[0]);
    EXPECT_EQ(byte { 0x01 }, value[1]);
    EXPECT_EQ(byte { 0xb0 }, value[2]);
    value = mode_pages.at(3);
    EXPECT_TRUE(value.empty());
}
