//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include "buses/bus_factory.h"
#include "controllers/controller_factory.h"
#include "shared_protobuf/command_context.h"
#include "shared_command/command_dispatcher.h"
#include "shared_command/image_support.h"
#include "shared_command/command_response.h"
#include "shared_command/command_executor.h"
#include "s2p_thread.h"
#include "generated/s2p_interface.pb.h"

using namespace std;

class S2p
{
    static const int DEFAULT_PORT = 6868;

public:

    S2p() = default;
    ~S2p() = default;

    int run(span<char*>, bool = false);

private:

    void Banner(span<char*>, bool) const;
    bool InitBus(bool);
    void CleanUp();
    void ReadAccessToken(const path&);
    void LogDevices(string_view) const;
    static void TerminationHandler(int);
    string ParseArguments(span<char*>, PbCommand&, int&, string&);
    void SetUpEnvironment();
    void ProcessScsiCommands();
    bool WaitForNotBusy() const;

    bool ExecuteCommand(CommandContext&);
    bool ExecuteWithLock(const CommandContext&);
    bool HandleDeviceListChange(const CommandContext&, PbOperation) const;

    static PbDeviceType ParseDeviceType(const string&);

    bool is_sasi = false;

    string access_token;

    S2pImage s2p_image;

    [[no_unique_address]] CommandResponse response;

    S2pThread service_thread;

    unique_ptr<CommandExecutor> executor;

    shared_ptr<CommandDispatcher> dispatcher;

    shared_ptr<ControllerFactory> controller_factory;

    unique_ptr<BusFactory> bus_factory;

    unique_ptr<Bus> bus;

    // Required for the termination handler
    static inline S2p *instance;
};
