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
    EXPECT_TRUE(message.contains("fehlgeschlagen"));

    message = command_localizer.Localize(ERROR_AUTHENTICATION, "en");
    EXPECT_TRUE(message.contains("failed"));

    message = command_localizer.Localize(ERROR_AUTHENTICATION, "fr");
    EXPECT_TRUE(message.contains("éronnée"));

    message = command_localizer.Localize(ERROR_AUTHENTICATION, "es");
    EXPECT_TRUE(message.contains("Fallo"));
}
