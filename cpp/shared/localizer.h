//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_set>
#include <regex>
#include "s2p_util.h"

using namespace std;

enum class LocalizationKey
{
    ERROR_AUTHENTICATION,
    ERROR_OPERATION,
    ERROR_LOG_LEVEL,
    ERROR_MISSING_DEVICE_ID,
    ERROR_MISSING_FILENAME,
    ERROR_DEVICE_MISSING_FILENAME,
    ERROR_IMAGE_IN_USE,
    ERROR_IMAGE_FILE_INFO,
    ERROR_RESERVED_ID,
    ERROR_NON_EXISTING_DEVICE,
    ERROR_NON_EXISTING_UNIT,
    ERROR_UNKNOWN_DEVICE_TYPE,
    ERROR_MISSING_DEVICE_TYPE,
    ERROR_DUPLICATE_ID,
    ERROR_DETACH,
    ERROR_EJECT_REQUIRED,
    ERROR_DEVICE_NAME_UPDATE,
    ERROR_SHUTDOWN_MODE_MISSING,
    ERROR_SHUTDOWN_MODE_INVALID,
    ERROR_SHUTDOWN_PERMISSION,
    ERROR_FILE_OPEN,
    ERROR_SCSI_LEVEL,
    ERROR_BLOCK_SIZE,
    ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
    ERROR_CONTROLLER,
    ERROR_INVALID_ID,
    ERROR_INVALID_LUN,
    ERROR_LUN0,
    ERROR_INITIALIZATION,
    ERROR_OPERATION_DENIED_STOPPABLE,
    ERROR_OPERATION_DENIED_REMOVABLE,
    ERROR_OPERATION_DENIED_PROTECTABLE,
    ERROR_OPERATION_DENIED_READY,
    ERROR_UNIQUE_DEVICE_TYPE
};

class Localizer
{
public:

    Localizer();
    ~Localizer() = default;

    string Localize(LocalizationKey, const string&, const string& = "", const string& = "", const string& = "") const;

private:

    void Add(LocalizationKey, const string&, string_view);
    unordered_map<string, unordered_map<LocalizationKey, string>, s2p_util::StringHash, equal_to<>> localized_messages;

    // Supported locales, always lower case
    unordered_set<string, s2p_util::StringHash, equal_to<>> supported_languages = { "en", "de", "sv", "fr", "es", "zh" };

    const regex regex1 = regex("%1");
    const regex regex2 = regex("%2");
    const regex regex3 = regex("%3");
};
