//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/logger_util.h"

using namespace logger_util;

TEST(LoggerUtilTest, CreateLogger)
{
    const auto l = CreateLogger("test");
    EXPECT_NE(nullptr, l);
    EXPECT_EQ(l, CreateLogger("test"));
}
