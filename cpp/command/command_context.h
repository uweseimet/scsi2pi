//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <spdlog/spdlog.h>
#include "command_localizer.h"
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace spdlog;
using namespace s2p_interface;

class CommandContext
{

public:

    CommandContext(const PbCommand &cmd, logger &l) : command(cmd), s2p_logger(l)
    {
    }
    CommandContext(int f, logger &l) : fd(f), s2p_logger(l)
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

    bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
    bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "",
        const string& = "") const;
    bool ReturnSuccessStatus() const;
    bool ReturnErrorStatus(const string&) const;

    logger& GetLogger() const
    {
        return s2p_logger;
    }

private:

    bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;

    PbCommand command;

    string locale;

    int fd = -1;

    logger &s2p_logger;
};
