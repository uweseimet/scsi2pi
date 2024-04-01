//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "command_executor.h"
#include "image_support.h"

class CommandDispatcher
{

public:

    CommandDispatcher(S2pImage &i, CommandExecutor &e) : s2p_image(i), executor(e)
    {
    }
    ~CommandDispatcher() = default;

    bool DispatchCommand(const CommandContext&, PbResult&);

    bool ShutDown(AbstractController::shutdown_mode) const;

    static bool SetLogLevel(const string&);

private:

    bool ExecuteWithLock(const CommandContext&);
    bool HandleDeviceListChange(const CommandContext&) const;
    bool ShutDown(const CommandContext&) const;

    S2pImage &s2p_image;

    CommandExecutor &executor;
};
