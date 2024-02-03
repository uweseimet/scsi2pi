//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2pctl/s2pctl_parser.h"

TEST(S2pCtlParserTest, ParseOperation)
{
    S2pCtlParser parser;

    EXPECT_EQ(ATTACH, parser.ParseOperation("A"));
    EXPECT_EQ(ATTACH, parser.ParseOperation("a"));
    EXPECT_EQ(DETACH, parser.ParseOperation("d"));
    EXPECT_EQ(INSERT, parser.ParseOperation("i"));
    EXPECT_EQ(EJECT, parser.ParseOperation("e"));
    EXPECT_EQ(PROTECT, parser.ParseOperation("p"));
    EXPECT_EQ(UNPROTECT, parser.ParseOperation("u"));
    EXPECT_EQ(NO_OPERATION, parser.ParseOperation(""));
    EXPECT_EQ(NO_OPERATION, parser.ParseOperation("xyz"));
}

TEST(S2pCtlParserTest, ParseDeviceType)
{
    S2pCtlParser parser;

    EXPECT_EQ(SCCD, parser.ParseDeviceType("sccd"));
    EXPECT_EQ(SCDP, parser.ParseDeviceType("scdp"));
    EXPECT_EQ(SCHD, parser.ParseDeviceType("schd"));
    EXPECT_EQ(SCLP, parser.ParseDeviceType("sclp"));
    EXPECT_EQ(SCMO, parser.ParseDeviceType("scmo"));
    EXPECT_EQ(SCRM, parser.ParseDeviceType("scrm"));
    EXPECT_EQ(SCHS, parser.ParseDeviceType("schs"));

    EXPECT_EQ(SCCD, parser.ParseDeviceType("c"));
    EXPECT_EQ(SCDP, parser.ParseDeviceType("d"));
    EXPECT_EQ(SCHD, parser.ParseDeviceType("h"));
    EXPECT_EQ(SCLP, parser.ParseDeviceType("l"));
    EXPECT_EQ(SCMO, parser.ParseDeviceType("m"));
    EXPECT_EQ(SCRM, parser.ParseDeviceType("r"));
    EXPECT_EQ(SCHS, parser.ParseDeviceType("s"));

    EXPECT_EQ(UNDEFINED, parser.ParseDeviceType(""));
    EXPECT_EQ(UNDEFINED, parser.ParseDeviceType("xyz"));
}
