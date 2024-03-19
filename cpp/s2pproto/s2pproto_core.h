//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>
#include "s2pproto_executor.h"

using namespace std;

class S2pProto
{

public:

    int Run(span<char*>, bool);

private:

    static void Banner(bool);

    bool Init(bool);
    bool ParseArguments(span<char*>);
    int GenerateOutput(const string&, const string&);

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<Bus> bus;

    unique_ptr<S2pProtoExecutor> executor;

    bool version = false;
    bool help = false;

    int initiator_id = -1;
    int target_id = -1;
    int target_lun = 0;

    string protobuf_input_filename;
    string protobuf_output_filename;

    S2pProtoExecutor::protobuf_format input_format = S2pProtoExecutor::protobuf_format::json;
    S2pProtoExecutor::protobuf_format output_format = S2pProtoExecutor::protobuf_format::json;

    string log_level = "info";

    // Required for the termination handler
    static inline S2pProto *instance;
};
