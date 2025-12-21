//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "shared/s2p_defs.h"
#include "generated/s2p_interface.pb.h"

class CommandContext;
class CommandExecutor;
class ControllerFactory;

using namespace std;
using namespace spdlog;
using namespace s2p_interface;

class CommandDispatcher
{

public:

    CommandDispatcher(CommandExecutor &e, ControllerFactory &f, logger &l) : executor(e), controller_factory(f), s2p_logger(
        l)
    {
    }
    ~CommandDispatcher() = default;

    bool DispatchCommand(const CommandContext&, PbResult&);

    bool ShutDown(ShutdownMode) const;

    bool SetLogLevel(const string&);

    bool SetWithoutTypes(const string&);

private:

    bool HandleDeviceListChange(const CommandContext&) const;
    bool ShutDown(const CommandContext&) const;

    CommandExecutor &executor;

    ControllerFactory &controller_factory;

    logger &s2p_logger;

    unordered_set<PbDeviceType> without_types;
};
