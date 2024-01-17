//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <filesystem>
#include <csignal>
#include <cstring>
#include <getopt.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/text_format.h>
#include <spdlog/spdlog.h>
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "s2pexec_core.h"

using namespace std;
using namespace filesystem;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace spdlog;
using namespace scsi_defs;
using namespace s2p_util;

void S2pExec::CleanUp() const
{
    if (bus) {
        bus->CleanUp();
    }
}

void S2pExec::TerminationHandler(int)
{
    instance->bus->SetRST(true);

    instance->CleanUp();

    // Process will terminate automatically
}

void S2pExec::Banner(span<char*> args, bool header)
{
    if (header) {
        cout << s2p_util::Banner("(SCSI Command Execution Tool)");
    }

    cout << "Usage: " << args[0]
        << " <--scsi-target/-s TARGET_ID>"
        << " <--input-file/-f INPUT_FILE> [--output-file/-F OUTPUT_FILE]"
        << " [--initiator-id/-i INITIATOR_ID]"
        << " [--log-level/-L LOG_LEVEL]"
        << " [--binary-input/-b] [--binary-output/-B] [--text-input/-t] [--text-output/-T]"
        << " [-X]\n"
        << " INITIATOR_ID is the s2pexec board ID (0-7). Default is 7.\n"
        << " TARGET_ID is the target device ID (0-7).\n"
        << " LUN is the optional target device LUN (0-31). Default is 0.\n"
        << " INPUT_FILE is the protobuf data input file, by default in JSON format.\n"
        << " OUTPUT_FILE is the protobuf data output file, by default in JSON format.\n"
        << " LOG_LEVEL is the log level (trace|debug|info|warning|error|off), default is 'info'.\n"
        << " --binary-input/-b Signals that the input file is in protobuf binary format instead of JSON format.\n"
        << " --text-intput/-t Signals that the input file is in protobuf text format instead of JSON format.\n"
        << " --binary-output/-B Generate a protobuf binary format file instead of a JSON file.\n"
        << " --text-output/-T Generate a protobuf text format file instead of a JSON file.\n"
        << " --shutdown/-X Shut down s2p running on the second board by sending a SCSI command.\n";
}

bool S2pExec::Init(bool)
{
    instance = this;
    // Signal handler for cleaning up
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    bus_factory = make_unique<BusFactory>();

    bus = bus_factory->CreateBus(false);
    if (bus) {
        scsi_executor = make_unique<S2pExecExecutor>(*bus, initiator_id);
    }

    return bus != nullptr;
}

bool S2pExec::ParseArguments(span<char*> args)
{
    static const struct option options[] = {
        { "binary-input", no_argument, nullptr, 'b' },
        { "binary-output", no_argument, nullptr, 'B' },
        { "input-file", required_argument, nullptr, 'f' },
        { "output-file", required_argument, nullptr, 'F' },
        { "help", no_argument, nullptr, 'h' },
        { "initiator-id", required_argument, nullptr, 'i' },
        { "log-level", required_argument, nullptr, 'L' },
        { "scsi-target", required_argument, nullptr, 's' },
        { "text-input", no_argument, nullptr, 't' },
        { "text-output", no_argument, nullptr, 'T' },
        { "version", no_argument, nullptr, 'v' },
        { "shutdown", no_argument, nullptr, 'X' },
        { nullptr, 0, nullptr, 0 }
    };

    string initiator = "7";
    string target;

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "bBf:F:hi:L:s:tTvX", options, nullptr)) != -1) {
        switch (opt) {
        case 'b':
            input_format = S2pExecExecutor::protobuf_format::binary;
            break;

        case 'B':
            output_format = S2pExecExecutor::protobuf_format::binary;
            break;

        case 'f':
            input_filename = optarg;
            break;

        case 'h':
            help = true;
            break;

        case 'i':
            initiator = optarg;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'o':
            output_filename = optarg;
            break;

        case 's':
            target = optarg;
            break;

        case 't':
            input_format = S2pExecExecutor::protobuf_format::text;
            break;

        case 'T':
            output_format = S2pExecExecutor::protobuf_format::text;
            break;

        case 'v':
            version = true;
            break;

        case 'X':
            shut_down = true;
            break;

        default:
            Banner(args, false);
            return false;
        }
    }

    if (help) {
        Banner(args, true);
        return true;
    }

    if (version) {
        cout << "s2pexec " << GetVersionString() << '\n';
        return true;
    }

    if (!SetLogLevel()) {
        throw parser_exception("Invalid log level: '" + log_level + "'");
    }

    if (!GetAsUnsignedInt(initiator, initiator_id) || initiator_id > 7) {
        throw parser_exception("Invalid initiator ID: '" + initiator + "' (0-7)");
    }

    if (const string error = ProcessId(8, 31, target, target_id, target_lun); !error.empty()) {
        throw parser_exception(error);
    }

    if (target_id == -1) {
        throw parser_exception("Missing target ID");
    }

    if (target_id == initiator_id) {
        throw parser_exception("Target ID and initiator ID must not be identical");
    }

    if (target_lun == -1) {
        target_lun = 0;
    }

    // A shutdown does not require an input file
    if (shut_down) {
        return true;
    }

    if (input_filename.empty()) {
        throw parser_exception("Missing input filename");
    }

    return true;
}

int S2pExec::Run(span<char*> args, bool in_process)
{
    if (args.size() < 2) {
        Banner(args, true);
        return EXIT_FAILURE;
    }

    try {
        if (!ParseArguments(args)) {
            return EXIT_FAILURE;
        }
        else if (version || help) {
            return EXIT_SUCCESS;
        }
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;
        return EXIT_FAILURE;
    }

    if (!in_process && !bus_factory->IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return EXIT_FAILURE;
    }

    scsi_executor->SetTarget(target_id, target_lun);

    if (shut_down) {
        const bool status = scsi_executor->ShutDown();
        if (!status) {
            cerr << "Error: Can't shut down SCSI2Pi instance " << target_id << ":" << target_lun << endl;
        }

        CleanUp();

        return status ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const int result = GenerateOutput(input_format, input_filename, output_format, output_filename);

    CleanUp();

    return result;
}

int S2pExec::GenerateOutput(S2pExecExecutor::protobuf_format input_format, const string &input_filename,
    S2pExecExecutor::protobuf_format output_format, const string &output_filename)
{
    PbResult result;
    if (string error = scsi_executor->Execute(input_filename, input_format, result); !error.empty()) {
        cerr << "Error: " << error << endl;

        return EXIT_FAILURE;
    }

    if (output_filename.empty()) {
        string json;
        (void)MessageToJsonString(result, &json);
        cout << json << '\n';

        return EXIT_SUCCESS;
    }

    switch (output_format) {
    case S2pExecExecutor::protobuf_format::binary: {
        ofstream out(output_filename, ios::binary);
        if (out.fail()) {
            cerr << "Error: " << "Can't open protobuf binary output file '" << output_filename << "'" << endl;
        }

        const string data = result.SerializeAsString();
        out.write(data.data(), data.size());
        break;
    }

    case S2pExecExecutor::protobuf_format::json: {
        ofstream out(output_filename);
        if (out.fail()) {
            cerr << "Error: " << "Can't open protobuf JSON output file '" << output_filename << "'" << endl;
        }

        string json;
        (void)MessageToJsonString(result, &json);
        out << json << '\n';
        break;
    }

    case S2pExecExecutor::protobuf_format::text: {
        ofstream out(output_filename);
        if (out.fail()) {
            cerr << "Error: " << "Can't open protobuf text format output file '" << output_filename << "'" << endl;
        }

        string text;
        TextFormat::PrintToString(result, &text);
        out << text << '\n';
        break;
    }

    default:
        assert(false);
        break;
    }

    return EXIT_SUCCESS;
}

bool S2pExec::SetLogLevel() const
{
    const level::level_enum l = level::from_str(log_level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != log_level) {
        return false;
    }

    set_level(l);

    return true;
}

