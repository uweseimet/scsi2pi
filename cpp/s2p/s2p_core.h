//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "buses/bus_factory.h"
#include "controllers/controller_factory.h"
#include "base/property_handler.h"
#include "protobuf/command_context.h"
#include "command/command_dispatcher.h"
#include "command/image_support.h"
#include "command/command_response.h"
#include "command/command_executor.h"
#include "s2p_parser.h"
#include "s2p_thread.h"

using namespace std;
using namespace s2p_interface;

class S2p
{

public:

    int Run(span<char*>, bool = false);

private:

    bool InitBus(bool, bool);
    void CleanUp();
    void ReadAccessToken(const path&);
    void LogDevices(string_view) const;
    static void TerminationHandler(int);
    void SetUpEnvironment();
    void LogProperties() const;
    void CreateDevices();
    void AttachDevices(PbCommand&);
    void ProcessScsiCommands();
    bool WaitForNotBusy() const;

    bool ExecuteCommand(CommandContext&);

    static bool CheckActive(const property_map&, const string&);
    static void SetDeviceProperties(PbDeviceDefinition&, const string&, const string&);
    static PbDeviceType ParseDeviceType(const string&);

    string access_token;

    [[no_unique_address]] S2pParser s2p_parser;

    S2pImage s2p_image;

    [[no_unique_address]] CommandResponse response;

    S2pThread service_thread;

    unique_ptr<CommandExecutor> executor;

    shared_ptr<CommandDispatcher> dispatcher;

    shared_ptr<ControllerFactory> controller_factory;

    unique_ptr<BusFactory> bus_factory;

    unique_ptr<Bus> bus;

    PropertyHandler &property_handler = PropertyHandler::Instance();

    // Required for the termination handler
    static inline S2p *instance;
};
