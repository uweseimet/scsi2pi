//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/shared_exceptions.h"
#include "s2p/s2p_parser.h"

// getopt() on BSD differs from Linux, so these tests cannot pass on BSD
#if !defined __FreeBSD__ && !defined __NetBSD__
void SetUpArgs(vector<char*> &args, const char *arg1, const char *arg2, const char *arg3 = nullptr, const char *arg4 =
    nullptr)
{
    args.clear();
    args.emplace_back(strdup("s2p"));
    args.emplace_back(strdup(arg1));
    args.emplace_back(strdup(arg2));
    if (arg3) {
        args.emplace_back(strdup(arg3));
    }
    if (arg4) {
        args.emplace_back(strdup(arg4));
    }
}

TEST(S2pParserTest, ParseArguments_SCSI2Pi)
{
    bool is_sasi = false;
    bool ignore_conf = true;
    S2pParser parser;

    vector<char*> args;
    args.emplace_back(strdup("s2p"));
    auto properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_TRUE(properties.empty());
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-p", "1");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("1", properties[PropertyHandler::PORT]);

    SetUpArgs(args, "-r", "ids");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("ids", properties[PropertyHandler::RESERVED_IDS]);

    SetUpArgs(args, "--locale", "locale");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("locale", properties[PropertyHandler::LOCALE]);

    SetUpArgs(args, "-C", "property_file");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("property_file", properties[PropertyHandler::PROPERTY_FILES]);

    SetUpArgs(args, "-F", "image_folder");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("image_folder", properties[PropertyHandler::IMAGE_FOLDER]);

    SetUpArgs(args, "-L", "log_level");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("log_level", properties[PropertyHandler::LOG_LEVEL]);

    SetUpArgs(args, "-P", "token_file");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("token_file", properties[PropertyHandler::TOKEN_FILE]);

    SetUpArgs(args, "-R", "scan_depth");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("scan_depth", properties[PropertyHandler::SCAN_DEPTH]);

    SetUpArgs(args, "-H0", "-h1");
    parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_TRUE(is_sasi);

    SetUpArgs(args, "-HD0", "-hd0:1");
    parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_TRUE(is_sasi);
    is_sasi = false;

    SetUpArgs(args, "-i0", "test1.hds", "-h1", "test2.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);
    is_sasi = false;

    SetUpArgs(args, "-i0", "-t", "sahd", "test.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);
    is_sasi = false;

    SetUpArgs(args, "-i0", "-b", "4096", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("4096", properties["device.0.block_size"]);
    EXPECT_EQ("test.hds", properties["device.0.params"]);

    SetUpArgs(args, "-i1:2", "-t", "SCHD", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("schd", properties["device.1:2.type"]);
    EXPECT_EQ("test.hds", properties["device.1:2.params"]);

    SetUpArgs(args, "-h2", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("sahd", properties["device.2.type"]);
    EXPECT_EQ("test.hds", properties["device.2.params"]);
    EXPECT_TRUE(is_sasi);
    is_sasi = false;

    SetUpArgs(args, "-i1", "-n", "a:b:c", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("a:b:c", properties["device.1.name"]);
    EXPECT_EQ("test.hds", properties["device.1.params"]);

    SetUpArgs(args, "-i0", "--scsi-level", "3", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("3", properties["device.0.scsi_level"]);
    EXPECT_EQ("test.hds", properties["device.0.params"]);

    SetUpArgs(args, "-i1", "--caching-mode", "linux", "test.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("linux", properties["device.1.caching_mode"]);
    EXPECT_EQ("test.hds", properties["device.1.params"]);

    SetUpArgs(args, "-c", "key1=value1", "-c", "key2=value2");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("value1", properties["key1"]);
    EXPECT_EQ("value2", properties["key2"]);

    SetUpArgs(args, "-c", "key=");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("", properties["key"]);

    SetUpArgs(args, "-c", "xyz");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);

    SetUpArgs(args, "-c", "=xyz");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);
}

TEST(S2pParserTest, ParseArguments_BlueSCSI)
{
    bool is_sasi = false;
    bool ignore_conf = true;
    S2pParser parser;

    vector<char*> args;

    SetUpArgs(args, "-B", "HD2.hds");
    auto properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD2.hds", properties["device.2.params"]);
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-B", "HD21.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("schd", properties["device.2:1.type"]);
    EXPECT_EQ("512", properties["device.2:1.block_size"]);
    EXPECT_EQ("HD21.hds", properties["device.2:1.params"]);
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-B", "HD20.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD20.hds", properties["device.2.params"]);
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-i", "5", "-B", "FD2.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("schd", properties["device.5.type"]);
    EXPECT_EQ("512", properties["device.5.block_size"]);
    EXPECT_EQ("FD2.hds", properties["device.5.params"]);
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-h", "5", "-B", "HD2.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("sahd", properties["device.5.type"]);
    EXPECT_EQ("512", properties["device.5.block_size"]);
    EXPECT_EQ("HD2.hds", properties["device.5.params"]);
    EXPECT_TRUE(is_sasi);
    is_sasi = false;

    SetUpArgs(args, "-B", "CD13.iso");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("sccd", properties["device.1:3.type"]);
    EXPECT_EQ("512", properties["device.1:3.block_size"]);
    EXPECT_EQ("CD13.iso", properties["device.1:3.params"]);

    SetUpArgs(args, "-B", "MO731.mos");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("scmo", properties["device.7:31.type"]);
    EXPECT_EQ("512", properties["device.7:31.block_size"]);
    EXPECT_EQ("MO731.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-B", "RE731_2048.mos");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("scrm", properties["device.7:31.type"]);
    EXPECT_EQ("2048", properties["device.7:31.block_size"]);
    EXPECT_EQ("RE731_2048.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-b", "512", "-B", "RE731_2048.mos");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("scrm", properties["device.7:31.type"]);
    EXPECT_EQ("512", properties["device.7:31.block_size"]) << "Explicit sector size provided";
    EXPECT_EQ("RE731_2048.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-B", "HD2_vendor:product:revision.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("vendor:product:revision", properties["device.2.name"]);
    EXPECT_EQ("HD2_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "-n", "v:p:r", "HD2_vendor:product:revision.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("v:p:r", properties["device.2.name"]) << "Explicit product data provided";
    EXPECT_EQ("HD2_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD2vendor:product:revision.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD2vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD2_4096_vendor:product:revision.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ("schd", properties["device.2.type"]);
    EXPECT_EQ("4096", properties["device.2.block_size"]);
    EXPECT_EQ("vendor:product:revision", properties["device.2.name"]);
    EXPECT_EQ("HD2_4096_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD1.hds", "-B", "RE131.hds");
    properties = parser.ParseArguments(args, is_sasi, ignore_conf);
    EXPECT_EQ(6UL, properties.size());
    EXPECT_EQ("schd", properties["device.1.type"]);
    EXPECT_EQ("512", properties["device.1.block_size"]);
    EXPECT_EQ("HD1.hds", properties["device.1.params"]);
    EXPECT_EQ("scrm", properties["device.1:31.type"]);
    EXPECT_EQ("512", properties["device.1:31.block_size"]);
    EXPECT_EQ("RE131.hds", properties["device.1:31.params"]);

    SetUpArgs(args, "-B", "H1.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);

    SetUpArgs(args, "-B", "TP5.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);

    SetUpArgs(args, "-B", "XX2.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);

    SetUpArgs(args, "-B", "HD.hds");
    EXPECT_THROW(parser.ParseArguments(args, is_sasi, ignore_conf), parser_exception);
}
#endif
