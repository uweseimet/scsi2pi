//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/s2p_exceptions.h"

using namespace s2p_util;

TEST(S2pUtilTest, Split)
{
    auto v = Split("this_is_a_test", '_');
    EXPECT_EQ(4U, v.size());
    EXPECT_EQ("this", v[0]);
    EXPECT_EQ("is", v[1]);
    EXPECT_EQ("a", v[2]);
    EXPECT_EQ("test", v[3]);
    v = Split("test", ':');
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ("test", v[0]);
    v = Split(":test", ':');
    EXPECT_EQ(2U, v.size());
    EXPECT_EQ("", v[0]);
    EXPECT_EQ("test", v[1]);
    v = Split("test:", ':');
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ("test", v[0]);
    v = Split(":", ':');
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ("", v[0]);
    v = Split("", ':');
    EXPECT_EQ(0U, v.size());

    v = Split("this:is:a:test", ':', 1);
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ("this:is:a:test", v[0]);
    v = Split("this:is:a:test", ':', 2);
    EXPECT_EQ(2U, v.size());
    EXPECT_EQ("this", v[0]);
    EXPECT_EQ("is:a:test", v[1]);

    v = Split("", ':', 1);
    EXPECT_EQ(1U, v.size());
}

TEST(S2pUtilTest, ToUpper)
{
    EXPECT_EQ("ABC", ToUpper("abc"));
}

TEST(S2pUtilTest, ToLower)
{
    EXPECT_EQ("abc", ToLower("ABC"));
}

TEST(S2pUtilTest, GetExtensionLowerCase)
{
    EXPECT_EQ("ext", GetExtensionLowerCase("test.ext"));
    EXPECT_EQ("ext", GetExtensionLowerCase("test.EXT"));
    EXPECT_EQ("ext", GetExtensionLowerCase("test.1.EXT"));
}

TEST(S2pUtilTest, ParseIdAndLun)
{
    int id = -1;
    int lun = -1;

    string error = ParseIdAndLun("", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("8", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("0:32", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("-1:", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("0:-1", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("a", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("a:0", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("0:a", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("0", id, lun);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(0, id);
    EXPECT_EQ(-1, lun);

    error = ParseIdAndLun("7:31", id, lun);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(7, id);
    EXPECT_EQ(31, lun);
}

TEST(S2pUtilTest, ParseAsUnsignedInt)
{
    EXPECT_EQ(-1, ParseAsUnsignedInt(""));
    EXPECT_EQ(-1, ParseAsUnsignedInt("xyz"));
    EXPECT_EQ(-1, ParseAsUnsignedInt("-2"));
    EXPECT_EQ(-1, ParseAsUnsignedInt("1234567898765432112345678987654321")) << "Value is out of range";
    EXPECT_EQ(0, ParseAsUnsignedInt("0"));
    EXPECT_EQ(1234, ParseAsUnsignedInt("1234"));
    EXPECT_EQ(1234, ParseAsUnsignedInt(" 1234 "));
}

TEST(S2pUtilTest, Banner)
{
    EXPECT_FALSE(Banner("Test").empty());
}

TEST(S2pUtilTest, GetInquiryProductData)
{
    vector<uint8_t> data(36);
    memcpy(data.data() + 8, "12345678", 8);
    memcpy(data.data() + 16, "1234567890123456", 16);
    memcpy(data.data() + 32, "1234", 4);
    const auto& [vendor, product, revision] = GetInquiryProductData(data);
    EXPECT_EQ("12345678", vendor);
    EXPECT_EQ("1234567890123456", product);
    EXPECT_EQ("1234", revision);
}

TEST(S2pUtilTest, GetScsiLevel)
{
    EXPECT_EQ("-", GetScsiLevel(0));
    EXPECT_EQ("SCSI-1-CCS", GetScsiLevel(1));
    EXPECT_EQ("SCSI-2", GetScsiLevel(2));
    EXPECT_EQ("SCSI-3 (SPC)", GetScsiLevel(3));
    EXPECT_EQ("SPC-2", GetScsiLevel(4));
    EXPECT_EQ("SPC-3", GetScsiLevel(5));
    EXPECT_EQ("SPC-4", GetScsiLevel(6));
    EXPECT_EQ("SPC-5", GetScsiLevel(7));
    EXPECT_EQ("SPC-6", GetScsiLevel(8));
}

TEST(S2pUtilTest, GetHexBytes)
{
    auto bytes = HexToBytes("");
    EXPECT_TRUE(bytes.empty());

    vector<byte> result;
    result.emplace_back(byte { 0xab });
    bytes = HexToBytes("ab");
    EXPECT_EQ(result, bytes);

    result.emplace_back(byte { 0xcd });
    bytes = HexToBytes("ab:cd");
    EXPECT_EQ(result, bytes);

    result.emplace_back(byte { 0x12 });
    bytes = HexToBytes("ab:cd12");
    EXPECT_EQ(result, bytes);

    bytes = HexToBytes("ab:cd\n12");
    EXPECT_EQ(result, bytes);

    EXPECT_THROW(HexToBytes("ab:cd12xx"), out_of_range);
    EXPECT_THROW(HexToBytes(":abcd12"), out_of_range);
    EXPECT_THROW(HexToBytes("abcd12:"), out_of_range);
    EXPECT_THROW(HexToBytes("ab::cd12"), out_of_range);
    EXPECT_THROW(HexToBytes("9"), out_of_range);
    EXPECT_THROW(HexToBytes("012"), out_of_range);
    EXPECT_THROW(HexToBytes("x0"), out_of_range);
    EXPECT_THROW(HexToBytes("0x"), out_of_range);
}

TEST(S2pUtilTest, HexToDec)
{
    EXPECT_EQ(0, HexToDec('0'));
    EXPECT_EQ(9, HexToDec('9'));
    EXPECT_EQ(10, HexToDec('a'));
    EXPECT_EQ(15, HexToDec('f'));
    EXPECT_EQ(-1, HexToDec('A'));
    EXPECT_EQ(-1, HexToDec('F'));
    EXPECT_EQ(-1, HexToDec('x'));
}

TEST(S2pUtilTest, Trim)
{
    EXPECT_EQ("", Trim(""));
    EXPECT_EQ("", Trim(" "));
    EXPECT_EQ("x", Trim("x"));
    EXPECT_EQ("x", Trim(" x"));
    EXPECT_EQ("x", Trim("x\r"));
    EXPECT_EQ("x", Trim("x "));
    EXPECT_EQ("x", Trim(" x "));
    EXPECT_EQ("x y", Trim("x y"));
}

TEST(S2pUtilTest, CreateLogger)
{
    const auto l = CreateLogger("test");
    EXPECT_NE(nullptr, l);
    EXPECT_EQ(l, CreateLogger("test"));
}
