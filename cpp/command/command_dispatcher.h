//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "protobuf/command_context.h"
#include "command_executor.h"
#include "image_support.h"
#include "command_response.h"
#include "generated/s2p_interface.pb.h"

using namespace std;

class CommandDispatcher
{

public:

    CommandDispatcher(S2pImage &i, CommandResponse &r, CommandExecutor &e)
    : s2p_image(i), response(r), executor(e)
    {
    }
    ~CommandDispatcher() = default;

    bool DispatchCommand(const CommandContext&, PbResult&, const string&);

    bool ShutDown(AbstractController::shutdown_mode) const;

    static bool SetLogLevel(const string&);

private:

    bool ExecuteWithLock(const CommandContext&);
    bool HandleDeviceListChange(const CommandContext&, PbOperation) const;
    bool ShutDown(const CommandContext&, const string&) const;

    S2pImage &s2p_image;

    CommandResponse &response;

    CommandExecutor &executor;
};
