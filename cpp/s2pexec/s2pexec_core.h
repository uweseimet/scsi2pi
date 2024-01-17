//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>
#include "buses/bus_factory.h"
#include "s2pexec_executor.h"

using namespace std;

class S2pExec
{

public:

    S2pExec() = default;
    ~S2pExec() = default;

    int Run(span<char*>, bool = false);

private:

    static void Banner(span<char*>, bool);

    bool Init(bool);
    bool ParseArguments(span<char*>);
    int GenerateOutput(S2pExecExecutor::protobuf_format, const string&, S2pExecExecutor::protobuf_format,
        const string&);

    bool SetLogLevel() const;

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<BusFactory> bus_factory;

    unique_ptr<Bus> bus;

    unique_ptr<S2pExecExecutor> scsi_executor;

    bool version = false;
    bool help = false;

    int initiator_id = -1;
    int target_id = -1;
    int target_lun = 0;

    string input_filename;
    string output_filename;

    S2pExecExecutor::protobuf_format input_format = S2pExecExecutor::protobuf_format::json;
    S2pExecExecutor::protobuf_format output_format = S2pExecExecutor::protobuf_format::json;

    bool shut_down = false;

    string log_level = "info";

    // Required for the termination handler
    static inline S2pExec *instance;
};
