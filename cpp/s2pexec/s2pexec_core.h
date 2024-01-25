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

    string ReadData();
    string WriteData(int);

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

    bool request_sense = true;

    bool hex_only = false;

    bool sasi = false;

    vector<uint8_t> buffer;

    string binary_input_filename;
    string binary_output_filename;
    string hex_input_filename;
    string hex_output_filename;

    string command;

    string log_level = "info";

    // Required for the termination handler
    static inline S2pExec *instance;

    inline static const int DEFAULT_BUFFER_SIZE = 4096;
};
