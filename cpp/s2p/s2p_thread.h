//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
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

using namespace std;
using namespace spdlog;

class S2pThread
{
    using callback = function<bool(CommandContext&)>;

public:

    string Init(int, const callback&, shared_ptr<logger>);
    void Start();
    void Stop();
    bool IsRunning() const;

private:

    void Execute() const;
    bool ExecuteCommand(int) const;

    callback exec;

#ifndef __APPLE__
    jthread service_thread;
#else
    thread service_thread;
#endif

    S2pServer server;

    shared_ptr<logger> s2p_logger;
};
