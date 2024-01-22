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

    int Run(span<char*>, bool = false);

private:

    static void Banner(bool);

    bool Init(bool);
    bool ParseArguments(span<char*>);
    string ExecuteCommand();
    int GenerateOutput(S2pExecExecutor::protobuf_format, const string&, S2pExecExecutor::protobuf_format,
        const string&);

    string ReadData();
    string WriteData(int);

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

    bool sasi = false;

    bool text_data = false;

    vector<uint8_t> buffer;

    string data_filename;

    string protobuf_input_filename;
    string protobuf_output_filename;

    S2pExecExecutor::protobuf_format input_format = S2pExecExecutor::protobuf_format::json;
    S2pExecExecutor::protobuf_format output_format = S2pExecExecutor::protobuf_format::json;

    string command;

    string log_level = "info";

    // Required for the termination handler
    static inline S2pExec *instance;
};
