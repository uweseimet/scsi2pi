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

TEST(S2pUtilTest, ProcessId)
{
    int id = -1;
    int lun = -1;

    string error = ProcessId("", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("8", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("0:32", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("-1:", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("0:-1", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("a", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("a:0", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("0:a", id, lun);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(-1, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("0", id, lun);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(0, id);
    EXPECT_EQ(-1, lun);

    error = ProcessId("7:31", id, lun);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(7, id);
    EXPECT_EQ(31, lun);
}

TEST(S2pUtilTest, GetAsUnsignedInt)
{
    int result;

    EXPECT_FALSE(GetAsUnsignedInt("", result));
    EXPECT_FALSE(GetAsUnsignedInt("xyz", result));
    EXPECT_FALSE(GetAsUnsignedInt("-1", result));
    EXPECT_FALSE(GetAsUnsignedInt("1234567898765432112345678987654321", result)) << "Value is out of range";
    EXPECT_TRUE(GetAsUnsignedInt("0", result));
    EXPECT_EQ(0, result);
    EXPECT_TRUE(GetAsUnsignedInt("1234", result));
    EXPECT_EQ(1234, result);
    EXPECT_TRUE(GetAsUnsignedInt(" 1234 ", result));
    EXPECT_EQ(1234, result);
}

TEST(S2pUtilTest, Banner)
{
    EXPECT_FALSE(Banner("Test").empty());
}

TEST(S2pUtilTest, GetScsiLevel)
{
    EXPECT_EQ("???", GetScsiLevel(0));
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
}

TEST(S2pUtilTest, FormatBytes)
{
    const string &str_all =
        R"(00000000  00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f  '................'
00000010  10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f  '................'
00000020  20:21:22:23:24:25:26:27:28:29:2a:2b:2c:2d:2e:2f  ' !"#$%&'()*+,-./'
00000030  30:31:32:33:34:35:36:37:38:39:3a:3b:3c:3d:3e:3f  '0123456789:;<=>?'
00000040  40:41:42:43:44:45:46:47:48:49:4a:4b:4c:4d:4e:4f  '@ABCDEFGHIJKLMNO'
00000050  50:51:52:53:54:55:56:57:58:59:5a:5b:5c:5d:5e:5f  'PQRSTUVWXYZ[\]^_'
00000060  60:61:62:63:64:65:66:67:68:69:6a:6b:6c:6d:6e:6f  '`abcdefghijklmno'
00000070  70:71:72:73:74:75:76:77:78:79:7a:7b:7c:7d:7e:7f  'pqrstuvwxyz{|}~.'
00000080  80:81:82:83:84:85:86:87:88:89:8a:8b:8c:8d:8e:8f  '................'
00000090  90:91:92:93:94:95:96:97:98:99:9a:9b:9c:9d:9e:9f  '................'
000000a0  a0:a1:a2:a3:a4:a5:a6:a7:a8:a9:aa:ab:ac:ad:ae:af  '................'
000000b0  b0:b1:b2:b3:b4:b5:b6:b7:b8:b9:ba:bb:bc:bd:be:bf  '................'
000000c0  c0:c1:c2:c3:c4:c5:c6:c7:c8:c9:ca:cb:cc:cd:ce:cf  '................'
000000d0  d0:d1:d2:d3:d4:d5:d6:d7:d8:d9:da:db:dc:dd:de:df  '................'
000000e0  e0:e1:e2:e3:e4:e5:e6:e7:e8:e9:ea:eb:ec:ed:ee:ef  '................'
000000f0  f0:f1:f2:f3:f4:f5:f6:f7:f8:f9:fa:fb:fc:fd:fe:ff  '................')";

    const string &str_partial =
        R"(00000000  40:41:42:43:44:45:46:47:48:49:4a:4b:4c:4d:4e     '@ABCDEFGHIJKLMN')";

    const string &str_hex_only =
        R"(40:41:42:43:44:45:46:47:48:49:4a:4b:4c:4d:4e)";

    vector<uint8_t> bytes;
    for (int i = 0; i < 256; i++) {
        bytes.emplace_back(i);
    }
    EXPECT_EQ(str_all, FormatBytes(bytes, static_cast<int>(bytes.size()), 0));

    bytes.clear();
    for (int i = 64; i < 79; i++) {
        bytes.emplace_back(i);
    }
    EXPECT_EQ(str_partial, FormatBytes(bytes, static_cast<int>(bytes.size()), 0));
    EXPECT_EQ(str_partial, FormatBytes(bytes, static_cast<int>(bytes.size()), 10000));

    EXPECT_EQ(str_hex_only, FormatBytes(bytes, static_cast<int>(bytes.size()), 0, true));
    EXPECT_EQ("40:41\n...", FormatBytes(bytes, static_cast<int>(bytes.size()), 2, true));
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
