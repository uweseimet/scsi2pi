//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <spdlog/spdlog.h>
#include "buses/bus_factory.h"
#include "shared/s2p_formatter.h"
#include "s2pexec_executor.h"

using namespace std;

class S2pExec
{
    class execution_exception : public runtime_error
    {
        using runtime_error::runtime_error;
    };

    enum class protobuf_format
    {
        binary = 0b001,
        json = 0b010,
        text = 0b100
    };

public:

    int Run(span<char*>, bool);

private:

    static void Banner(bool, bool);

    bool Init(bool);
    bool ParseArguments(span<char*>, bool);
    bool RunInteractive(bool);
    int Run();

    tuple<sense_key, asc, int> ExecuteCommand();

    string ReadData();
    string WriteData(span<const uint8_t>);
    string ConvertData(const string&);

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<S2pExecExecutor> executor;

    S2pFormatter formatter;

    bool version = false;
    bool help = false;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    int timeout = 3;

    string log_limit = "128";

    bool request_sense = true;

    bool reset_bus = false;

    bool hex_only = false;

    bool sasi = false;

    bool use_sg = false;

    vector<uint8_t> buffer;

    string binary_input_filename;
    string binary_output_filename;
    string hex_input_filename;
    string hex_output_filename;

    string command;
    string data;

    shared_ptr<logger> s2pexec_logger;
    string log_level;

    string last_input;

    string device_file;

    string prompt = APP_NAME;

    // Required for the termination handler
    inline static S2pExec *instance;

    static const int DEFAULT_BUFFER_SIZE = 131072;

    inline static const string APP_NAME = "s2pexec";
};
