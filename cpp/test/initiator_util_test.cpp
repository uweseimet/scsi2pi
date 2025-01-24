//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/sinks/stdout_color_sinks.h>
#include "mocks.h"
#include "initiator/initiator_util.h"

using namespace initiator_util;

TEST(InitiatorUtilTest, ResetBus)
{
    NiceMock<MockBus> bus;

    EXPECT_CALL(bus, Reset);
    ResetBus(bus);
}

TEST(InitiatorUtilTest, SetLogLevel)
{
    const auto logger = stdout_color_st("initiator_util_test");
    EXPECT_TRUE(SetLogLevel(*logger, "error"));
    EXPECT_EQ(level::level_enum::err, logger->level());
    EXPECT_FALSE(SetLogLevel(*logger, "abc"));
    EXPECT_EQ(level::level_enum::err, logger->level());
    EXPECT_TRUE(SetLogLevel(*logger, ""));
    EXPECT_EQ(level::level_enum::err, logger->level());
}
