//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/localizer.h"
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class CommandContext
{

public:

    CommandContext(const PbCommand &cmd, string_view l) : command(cmd), locale(l)
    {
    }
    explicit CommandContext(int f) : fd(f)
    {
    }
    ~CommandContext() = default;

    void SetLocale(string_view l)
    {
        locale = l;
    }
    bool ReadCommand();
    void WriteResult(const PbResult&) const;
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

private:

    bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;

    const Localizer localizer;

    PbCommand command;

    string locale;

    int fd = -1;
};
