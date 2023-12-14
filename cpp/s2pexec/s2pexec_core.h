//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <span>
#include <vector>
#include "buses/bus_factory.h"
#include "s2pexec_executor.h"

using namespace std;

class ScsiExec
{

public:

    ScsiExec() = default;
    ~ScsiExec() = default;

    int run(span<char*>, bool = false);

private:

    bool Banner(span<char*>) const;
    bool Init(bool);
    void ParseArguments(span<char*>);

    bool SetLogLevel() const;

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<BusFactory> bus_factory;

    unique_ptr<Bus> bus;

    unique_ptr<S2pDumpExecutor> scsi_executor;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    string input_filename;
    string output_filename;

    S2pDumpExecutor::protobuf_format input_format = S2pDumpExecutor::protobuf_format::json;
    S2pDumpExecutor::protobuf_format output_format = S2pDumpExecutor::protobuf_format::json;

    bool shut_down = false;

    string log_level = "info";

    // Required for the termination handler
    static inline ScsiExec *instance;
};
