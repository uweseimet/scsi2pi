//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "base/property_handler.h"
#include "s2p/s2p_parser.h"
#include "shared/s2p_exceptions.h"
#include "generated/s2p_interface.pb.h"

using namespace s2p_interface;
using namespace s2p_parser;

void SetUpArgs(vector<char*> &args, const string &arg1, const string &arg2, const string &arg3 = "",
    const string &arg4 = "")
{
    for (char *arg : args) {
        free(arg); // NOSONAR free() must be used here because of allocation with strdup()
    }
    args.clear();
    args.emplace_back(strdup("s2p"));
    args.emplace_back(strdup(arg1.c_str()));
    args.emplace_back(strdup(arg2.c_str()));
    if (!arg3.empty()) {
        args.emplace_back(strdup(arg3.c_str()));
    }
    if (!arg4.empty()) {
        args.emplace_back(strdup(arg4.c_str()));
    }
}

TEST(S2pParserTest, ParseArguments_SCSI2Pi)
{
    bool ignore_conf = true;

    vector<char*> args;
    args.emplace_back(strdup("s2p"));
    auto properties = ParseArguments(args, ignore_conf);
    EXPECT_TRUE(properties.empty());

    SetUpArgs(args, "-p", "1");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("1", properties[PropertyHandler::PORT]);

    SetUpArgs(args, "-r", "ids");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("ids", properties[PropertyHandler::RESERVED_IDS]);

    SetUpArgs(args, "--locale", "locale");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("locale", properties[PropertyHandler::LOCALE]);

    SetUpArgs(args, "-C", "property_files");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("property_files", properties[PropertyHandler::PROPERTY_FILES]);

    SetUpArgs(args, "-F", "image_folder");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("image_folder", properties[PropertyHandler::IMAGE_FOLDER]);

    SetUpArgs(args, "-L", "log_level");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("log_level", properties[PropertyHandler::LOG_LEVEL]);

    SetUpArgs(args, "-l", "log_pattern");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("log_pattern", properties[PropertyHandler::LOG_PATTERN]);

    SetUpArgs(args, "--log-limit", "log_limit");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("log_limit", properties[PropertyHandler::LOG_LIMIT]);

    SetUpArgs(args, "-P", "token_file");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("token_file", properties[PropertyHandler::TOKEN_FILE]);

    SetUpArgs(args, "-R", "scan_depth");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("scan_depth", properties[PropertyHandler::SCAN_DEPTH]);

    SetUpArgs(args, "-s", "script_file");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("script_file", properties[PropertyHandler::SCRIPT_FILE]);

    SetUpArgs(args, "-i0", "-b", "4096", "test.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("4096", properties["device.0.block_size"]);
    EXPECT_EQ("test.hds", properties["device.0.params"]);

    SetUpArgs(args, "-i1:2", "-t", "SCHD", "test.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("schd", properties["device.1:2.type"]);
    EXPECT_EQ("test.hds", properties["device.1:2.params"]);

    SetUpArgs(args, "-ID1:0", "-n", "a:b:c", "test.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("a:b:c", properties["device.1:0.name"]);
    EXPECT_EQ("test.hds", properties["device.1:0.params"]);

    SetUpArgs(args, "-i0", "--scsi-level", "3", "test.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("3", properties["device.0.scsi_level"]);
    EXPECT_EQ("test.hds", properties["device.0.params"]);

    SetUpArgs(args, "-i1", "--caching-mode", "linux", "test.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("linux", properties["device.1.caching_mode"]);
    EXPECT_EQ("test.hds", properties["device.1.params"]);

    SetUpArgs(args, "-c", "key1=value1", "-c", "key2=value2");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(2UL, properties.size());
    EXPECT_EQ("value1", properties["key1"]);
    EXPECT_EQ("value2", properties["key2"]);

    SetUpArgs(args, "-c", "key=");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(1UL, properties.size());
    EXPECT_EQ("", properties["key"]);

    SetUpArgs(args, "-c", "xyz");
    EXPECT_THROW(ParseArguments(args, ignore_conf), ParserException);

    SetUpArgs(args, "-c", "=xyz");
    EXPECT_THROW(ParseArguments(args, ignore_conf), ParserException);

    for (char *arg : args) {
        free(arg); // NOSONAR free() must be used here because of allocation with strdup()
    }
}

TEST(S2pParserTest, ParseArguments_BlueSCSI)
{
    bool ignore_conf = true;

    vector<char*> args;

    SetUpArgs(args, "-B", "HD2.hds");
    auto properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD2.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD21.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2:1.type"]);
    EXPECT_EQ("512", properties["device.2:1.block_size"]);
    EXPECT_EQ("HD21.hds", properties["device.2:1.params"]);

    SetUpArgs(args, "-B", "HD20.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD20.hds", properties["device.2.params"]);

    SetUpArgs(args, "-i", "5", "-B", "FD2.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.5.type"]);
    EXPECT_EQ("512", properties["device.5.block_size"]);
    EXPECT_EQ("FD2.hds", properties["device.5.params"]);

    SetUpArgs(args, "-B", "CD13.iso");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCCD), properties["device.1:3.type"]);
    EXPECT_EQ("512", properties["device.1:3.block_size"]);
    EXPECT_EQ("CD13.iso", properties["device.1:3.params"]);

    SetUpArgs(args, "-B", "MO731.mos");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCMO), properties["device.7:31.type"]);
    EXPECT_EQ("512", properties["device.7:31.block_size"]);
    EXPECT_EQ("MO731.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-B", "RE731_2048.mos");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCRM), properties["device.7:31.type"]);
    EXPECT_EQ("2048", properties["device.7:31.block_size"]);
    EXPECT_EQ("RE731_2048.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-b", "512", "-B", "RE731_2048.mos");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCRM), properties["device.7:31.type"]);
    EXPECT_EQ("512", properties["device.7:31.block_size"]) << "Explicit sector size provided";
    EXPECT_EQ("RE731_2048.mos", properties["device.7:31.params"]);

    SetUpArgs(args, "-B", "HD2_vendor:product:revision.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("vendor:product:revision", properties["device.2.name"]);
    EXPECT_EQ("HD2_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "-n", "v:p:r", "HD2_vendor:product:revision.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("v:p:r", properties["device.2.name"]) << "Explicit product data provided";
    EXPECT_EQ("HD2_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD2vendor:product:revision.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("512", properties["device.2.block_size"]);
    EXPECT_EQ("HD2vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD2_4096_vendor:product:revision.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(4UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.2.type"]);
    EXPECT_EQ("4096", properties["device.2.block_size"]);
    EXPECT_EQ("vendor:product:revision", properties["device.2.name"]);
    EXPECT_EQ("HD2_4096_vendor:product:revision.hds", properties["device.2.params"]);

    SetUpArgs(args, "-B", "HD1.hds", "-B", "RE131.hds");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(6UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCHD), properties["device.1.type"]);
    EXPECT_EQ("512", properties["device.1.block_size"]);
    EXPECT_EQ("HD1.hds", properties["device.1.params"]);
    EXPECT_EQ(PbDeviceType_Name(SCRM), properties["device.1:31.type"]);
    EXPECT_EQ("512", properties["device.1:31.block_size"]);
    EXPECT_EQ("RE131.hds", properties["device.1:31.params"]);

    SetUpArgs(args, "-B", "TP73.tap");
    properties = ParseArguments(args, ignore_conf);
    EXPECT_EQ(3UL, properties.size());
    EXPECT_EQ(PbDeviceType_Name(SCTP), properties["device.7:3.type"]);
    EXPECT_EQ("512", properties["device.7:3.block_size"]);
    EXPECT_EQ("TP73.tap", properties["device.7:3.params"]);

    SetUpArgs(args, "-B", "H1.hds");
    EXPECT_THROW(ParseArguments(args, ignore_conf), ParserException);

    SetUpArgs(args, "-B", "XX2.hds");
    EXPECT_THROW(ParseArguments(args, ignore_conf), ParserException);

    SetUpArgs(args, "-B", "HD.hds");
    EXPECT_THROW(ParseArguments(args, ignore_conf), ParserException);

    for (char *arg : args) {
        free(arg); // NOSONAR free() must be used here because of allocation with strdup()
    }
}
