//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>

using namespace std;

enum class Language
{
    DE,
    EN,
    ES,
    FR
};

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
    ERROR_UNIQUE_SCDP,
    ERROR_PERSIST
};

class CommandLocalizer final
{

public:

    CommandLocalizer();
    ~CommandLocalizer() = default;

    template<typename ... Args>
    string Localize(LocalizationKey key, string_view locale, Args &&... args) const
    {
        return LocalizeImpl(key, locale, fmt::make_format_args(args...));
    }

private:

    static constexpr string_view ToStringView(Language language)
    {
        switch (language) {
        case Language::DE:
            return "de";

        case Language::ES:
            return "es";

        case Language::FR:
            return "fr";

        case Language::EN:
        default:
            return "en";
        }
    }

    static constexpr Language ToLanguage(string_view locale)
    {
        if (locale.size() >= 2) {
            const auto c0 = static_cast<char>(tolower(static_cast<unsigned char>(locale[0])));
            const auto c1 = static_cast<char>(tolower(static_cast<unsigned char>(locale[1])));

            if (c0 == 'd' && c1 == 'e') {
                return Language::DE;
            }

            if (c0 == 'f' && c1 == 'r') {
                return Language::FR;
            }

            if (c0 == 'e' && c1 == 's') {
                return Language::ES;
            }
        }

        return Language::EN;
    }

    void Add(LocalizationKey, Language, string_view);

    string LocalizeImpl(LocalizationKey, string_view, fmt::format_args) const;

    unordered_map<Language, unordered_map<LocalizationKey, string>> localized_messages;

    inline static constexpr array<string_view, 4> SUPPORTED_LOCALES = { "de", "en", "es", "fr" };
};
