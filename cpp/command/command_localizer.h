//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <regex>
#include "shared/s2p_util.h"

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
    ERROR_SHUTDOWN_MODE_INVALID,
    ERROR_SHUTDOWN_PERMISSION,
    ERROR_FILE_OPEN,
    ERROR_SCSI_LEVEL,
    ERROR_BLOCK_SIZE,
    ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
    ERROR_CONTROLLER,
    ERROR_INVALID_ID,
    ERROR_INVALID_LUN,
    ERROR_MISSING_LUN0,
    ERROR_LUN0,
    ERROR_INITIALIZATION,
    ERROR_OPERATION_DENIED_STOPPABLE,
    ERROR_OPERATION_DENIED_REMOVABLE,
    ERROR_OPERATION_DENIED_PROTECTABLE,
    ERROR_OPERATION_DENIED_READY,
    ERROR_UNIQUE_DEVICE_TYPE,
    ERROR_PERSIST
};

class CommandLocalizer
{

public:

    CommandLocalizer();
    ~CommandLocalizer() = default;

    string Localize(LocalizationKey, const string&, const string& = "", const string& = "", const string& = "") const;

private:

    void Add(LocalizationKey, const string&, string_view);

    unordered_map<string, unordered_map<LocalizationKey, string>, s2p_util::StringHash, equal_to<>> localized_messages;

    inline static const regex REGEX1 = regex("%1");
    inline static const regex REGEX2 = regex("%2");
    inline static const regex REGEX3 = regex("%3");
};
