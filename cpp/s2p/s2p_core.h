//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include "command/command_dispatcher.h"
#include "base/property_handler.h"
#include "s2p_thread.h"

using namespace filesystem;
using namespace s2p_interface;

class S2p
{

public:

    int Run(span<char*>, bool = false, bool = false);

private:

    bool InitBus(bool, bool);
    void CleanUp();
    void ReadAccessToken(const path&);
    void LogDevices(string_view) const;
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

    string access_token;

    S2pThread service_thread;

    ControllerFactory controller_factory;

    shared_ptr<CommandDispatcher> dispatcher;

    unique_ptr<CommandExecutor> executor;

    unique_ptr<Bus> bus;

    PropertyHandler &property_handler = PropertyHandler::Instance();

    // Required for the termination handler
    static inline S2p *instance;
};
