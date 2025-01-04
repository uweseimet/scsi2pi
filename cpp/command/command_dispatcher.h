//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "command_executor.h"

class CommandDispatcher
{

public:

    explicit CommandDispatcher(CommandExecutor &e, ControllerFactory &f, logger &l) : executor(e), controller_factory(
        f), s2p_logger(l)
    {
    }
    ~CommandDispatcher() = default;

    bool DispatchCommand(const CommandContext&, PbResult&);

    bool ShutDown(ShutdownMode) const;

    bool SetLogLevel(const string&);

private:

    bool ExecuteWithLock(const CommandContext&);
    bool HandleDeviceListChange(const CommandContext&) const;
    bool ShutDown(const CommandContext&) const;

    CommandExecutor &executor;

    ControllerFactory &controller_factory;

    logger &s2p_logger;
};
