//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

// Note that this test depends on no conflicting global properties being defined in /etc/s2p.conf

#include <gtest/gtest.h>
#include "shared/s2p_exceptions.h"
#include "test_shared.h"

using namespace testing;

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

    PropertyHandler::GetInstance().Init("", { }, true);
    EXPECT_THROW(PropertyHandler::GetInstance().Init("non_existing_file", { }, true), ParserException);

    property_map cmd_properties;
    cmd_properties["key1"] = "value2";
    cmd_properties["device.1.params"] = "params1";
    cmd_properties["device.2:1.params"] = "params2";
    SetUpProperties(properties1, properties2, cmd_properties);
    EXPECT_EQ("value2", PropertyHandler::GetInstance().RemoveProperty("key1"));
    EXPECT_EQ("value2", PropertyHandler::GetInstance().RemoveProperty("key2"));
    EXPECT_EQ("value3", PropertyHandler::GetInstance().RemoveProperty("key3"));
    EXPECT_EQ("params1", PropertyHandler::GetInstance().RemoveProperty("device.1:0.params"));
    EXPECT_EQ("params2", PropertyHandler::GetInstance().RemoveProperty("device.2:1.params"));
    EXPECT_EQ("params3", PropertyHandler::GetInstance().RemoveProperty("device.3:0.params"));

    EXPECT_THROW(SetUpProperties(properties3), ParserException);
}

TEST(PropertyHandlerTest, GetProperties)
{
    const string &properties =
        R"(key1=value1
key2=value2
key11=value2
)";

    SetUpProperties(properties);

    auto p = PropertyHandler::GetInstance().GetProperties("key2");
    EXPECT_EQ(1U, p.size());
    EXPECT_TRUE(p.contains("key2"));

    p = PropertyHandler::GetInstance().GetProperties("key1");
    EXPECT_EQ(2U, p.size());
    EXPECT_TRUE(p.contains("key1"));
    EXPECT_TRUE(p.contains("key11"));
}

TEST(PropertyHandlerTest, RemoveProperty)
{
    const string &properties =
        R"(key1=value1
key2=value2
)";

    SetUpProperties(properties);

    EXPECT_TRUE(PropertyHandler::GetInstance().RemoveProperty("key").empty());
    EXPECT_TRUE(PropertyHandler::GetInstance().RemoveProperty("key3").empty());
    EXPECT_EQ("value1", PropertyHandler::GetInstance().RemoveProperty("key1"));
    EXPECT_EQ("value2", PropertyHandler::GetInstance().RemoveProperty("key2"));
    EXPECT_EQ(2U, PropertyHandler::GetInstance().GetProperties().size());
    EXPECT_TRUE(PropertyHandler::GetInstance().GetUnknownProperties().empty());

    EXPECT_EQ("default_value", PropertyHandler::GetInstance().RemoveProperty("key", "default_value"));
}
