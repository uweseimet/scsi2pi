//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "command_localizer.h"
#include "generated/s2p_interface.pb.h"

using namespace spdlog;
using namespace s2p_interface;

class CommandContext final
{

public:

    CommandContext(const PbCommand &cmd, logger &l) : command(cmd), context_logger(l)
    {
    }
    CommandContext(int f, logger &l) : fd(f), context_logger(l)
    {
    }
    ~CommandContext() = default;

    void SetLocale(string_view l)
    {
        locale = l;
    }
    bool ReadCommand();
    bool WriteResult(const PbResult&) const;
    bool WriteSuccessResult(PbResult&) const;
    const PbCommand& GetCommand() const
    {
        return command;
    }

    bool ReturnSuccessStatus() const;
    bool ReturnErrorStatus(const string&) const;

    template<typename ... Args>
    bool ReturnLocalizedError(LocalizationKey key, Args &&... args) const
    {
        return ReturnLocalizedError(key, PbErrorCode::NO_ERROR_CODE, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    bool ReturnLocalizedError(LocalizationKey key, PbErrorCode error_code, Args &&... args) const
    {
        static const CommandLocalizer command_localizer;

        if (error_code == PbErrorCode::UNKNOWN_OPERATION) {
            context_logger.trace(command_localizer.Localize(key, "en", std::as_const(args)...));
        }
        else {
            context_logger.error(command_localizer.Localize(key, "en", std::as_const(args)...));
        }

        return ReturnStatus(false, command_localizer.Localize(key, locale, std::forward<Args>(args)...), error_code,
            false);
    }

    logger& GetLogger() const
    {
        return context_logger;
    }

private:

    bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;

    PbCommand command;

    string locale;

    int fd = -1;

    logger &context_logger;
};
