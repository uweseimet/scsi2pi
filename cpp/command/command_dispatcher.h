//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "command_executor.h"

class CommandDispatcher
{

public:

    explicit CommandDispatcher(CommandExecutor &e) : executor(e)
    {
    }
    ~CommandDispatcher() = default;

    bool DispatchCommand(const CommandContext&, PbResult&);

    bool ShutDown(shutdown_mode) const;

    static bool SetLogLevel(const string&);

private:

    bool ExecuteWithLock(const CommandContext&);
    bool HandleDeviceListChange(const CommandContext&) const;
    bool ShutDown(const CommandContext&) const;

    CommandExecutor &executor;
};
