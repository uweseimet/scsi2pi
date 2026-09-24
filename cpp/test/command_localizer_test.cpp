//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "command/command_localizer.h"

using enum LocalizationKey;

TEST(CommandLocalizer, Localize)
{
    CommandLocalizer command_localizer;

    string message = command_localizer.Localize(ERROR_AUTHENTICATION, "");
    EXPECT_FALSE(message.contains("enum value"));

    message = command_localizer.Localize(ERROR_AUTHENTICATION, "de_DE");
    EXPECT_FALSE(message.empty());
    EXPECT_FALSE(message.contains("enum value"));

    message = command_localizer.Localize(ERROR_AUTHENTICATION, "en");
    EXPECT_FALSE(message.empty());
    EXPECT_FALSE(message.contains("enum value"));
}
