//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <spdlog/spdlog.h>
#include "s2p_server.h"

class CommandContext;

using namespace spdlog;

class S2pThread final
{
    using callback = function<bool(CommandContext&)>;

public:

    string Init(int, const callback&, shared_ptr<logger>);
    void Start();
    void Stop();
    bool IsRunning() const;

private:

    void Execute();
    bool ExecuteCommand(int);

    callback exec;

    jthread service_thread;

    S2pServer server;

    shared_ptr<logger> s2p_logger;
};
