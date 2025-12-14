//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include "buses/bus.h"
#include "s2pproto_executor.h"

using namespace std;

class S2pProto
{

public:

    int Run(span<char*>, bool, bool = false);

private:

    static void Banner(bool);

    bool Init(bool, bool);
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

    ProtobufFormat input_format = ProtobufFormat::JSON;
    ProtobufFormat output_format = ProtobufFormat::JSON;

    string log_level = "info";

    // Required for the termination handler
    inline static S2pProto *instance;

    inline static const string APP_NAME = "s2pproto";
};
