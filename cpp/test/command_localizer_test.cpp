//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "command/command_localizer.h"

TEST(CinnabdLocalizer, Localize)
{
    CommandLocalizer command_localizer;

    string message = command_localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "");
    EXPECT_FALSE(message.empty());
    EXPECT_EQ(string::npos, message.find("enum value"));

    message = command_localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "de_DE");
    EXPECT_FALSE(message.empty());
    EXPECT_EQ(string::npos, message.find("enum value"));

    message = command_localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "en");
    EXPECT_FALSE(message.empty());
    EXPECT_EQ(string::npos, message.find("enum value"));

    message = command_localizer.Localize((LocalizationKey)1234, "");
    EXPECT_FALSE(message.empty());
    EXPECT_NE(string::npos, message.find("enum value"));
}
