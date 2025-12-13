//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include "base/property_handler.h"
#include "buses/bus.h"
#include "command/command_executor.h"
#include "controllers/controller_factory.h"
#include "s2p_thread.h"

using namespace filesystem;

class CommandDispatcher;

class S2p
{

public:

    int Run(span<char*>, bool = false, bool = false);

    bool Ready() const;

private:

    bool InitBus(bool, bool);
    void CleanUp();
    void ReadAccessToken(const path&);
    void LogDevices(const string&) const;
    bool ParseProperties(const property_map&, int&, bool);
    void SetUpEnvironment();
    string MapExtensions() const;
    void LogProperties() const;
    void CreateDevices();
    void AttachInitialDevices(PbCommand&);
    void ProcessScsiCommands();
    bool WaitForNotBusy() const;

    bool ExecuteCommand(CommandContext&);

    void SetDeviceProperties(PbDeviceDefinition&, const string&, const string&) const;

    static bool CheckActive(const property_map&, const string&);

    static void TerminationHandler(int);

    bool ready = false;

    string access_token;

    S2pThread service_thread;

    ControllerFactory controller_factory;

    shared_ptr<CommandDispatcher> dispatcher;

    unique_ptr<CommandExecutor> executor;

    unique_ptr<Bus> bus;

    PropertyHandler &property_handler = PropertyHandler::GetInstance();

    // Required for the termination handler
    inline static S2p *instance;

    string log_level;

    shared_ptr<logger> s2p_logger;

    inline static const string APP_NAME = "s2p";
};
