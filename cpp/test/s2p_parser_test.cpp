//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/shared_exceptions.h"
#include "s2p/s2p_parser.h"

void SetUpArgs(vector<char*> &args, const char *arg1, const char *arg2, const char *arg3 = nullptr, const char *arg4 =
    nullptr)
{
    args.clear();
    args.emplace_back(strdup("arg0"));
    args.emplace_back(strdup(arg1));
    args.emplace_back(strdup(arg2));
    if (arg3) {
        args.emplace_back(strdup(arg3));
    }
    if (arg4) {
        args.emplace_back(strdup(arg4));
    }
}

TEST(S2pParserTest, ParseArguments)
{
    bool is_sasi;
    S2pParser parser;

    vector<char*> args;
    args.emplace_back(strdup("arg0"));
    auto properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(1, properties.size());
    EXPECT_TRUE(properties.at(PropertyHandler::PROPERTY_FILE).empty());
    EXPECT_FALSE(is_sasi);

    SetUpArgs(args, "-p", "1");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("1", properties.at(PropertyHandler::PORT));

    SetUpArgs(args, "-r", "ids");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("ids", properties.at(PropertyHandler::RESERVED_IDS));

    SetUpArgs(args, "-z", "locale");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("locale", properties.at(PropertyHandler::LOCALE));

    SetUpArgs(args, "-C", "property_file");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(1, properties.size());
    EXPECT_EQ("property_file", properties.at(PropertyHandler::PROPERTY_FILE));

    SetUpArgs(args, "-F", "image_folder");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("image_folder", properties.at(PropertyHandler::IMAGE_FOLDER));

    SetUpArgs(args, "-L", "log_level");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("log_level", properties.at(PropertyHandler::LOG_LEVEL));

    SetUpArgs(args, "-P", "token_file");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("token_file", properties.at(PropertyHandler::TOKEN_FILE));

    SetUpArgs(args, "-R", "scan_depth");
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(2, properties.size());
    EXPECT_EQ("scan_depth", properties.at(PropertyHandler::SCAN_DEPTH));

    SetUpArgs(args, "-H0", "-h1");
    is_sasi = false;
    parser.ParseArguments(args, is_sasi);
    EXPECT_TRUE(is_sasi);

    SetUpArgs(args, "-HD0", "-hd0:1");
    is_sasi = false;
    parser.ParseArguments(args, is_sasi);
    EXPECT_TRUE(is_sasi);

    SetUpArgs(args, "-i0", "-h1");
    is_sasi = false;
    EXPECT_THROW(parser.ParseArguments(args, is_sasi), parser_exception);

    SetUpArgs(args, "-i0", "-b", "4096", "test.hds");
    is_sasi = false;
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(3, properties.size());
    EXPECT_EQ("4096", properties.at("device.0.block_size"));
    EXPECT_EQ("test.hds", properties.at("device.0.params"));

    SetUpArgs(args, "-i1:2", "-t", "schd", "test");
    is_sasi = false;
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(3, properties.size());
    EXPECT_EQ("schd", properties.at("device.1:2.type"));
    EXPECT_EQ("test", properties.at("device.1:2.params"));

    SetUpArgs(args, "-h2", "test");
    is_sasi = false;
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(3, properties.size());
    EXPECT_EQ("sahd", properties.at("device.2.type"));
    EXPECT_EQ("test", properties.at("device.2.params"));

    SetUpArgs(args, "-i1", "-n", "a:b:c", "test");
    is_sasi = false;
    properties = parser.ParseArguments(args, is_sasi);
    EXPECT_EQ(3, properties.size());
    EXPECT_EQ("a:b:c", properties.at("device.1.product_data"));
    EXPECT_EQ("test", properties.at("device.1.params"));
}
