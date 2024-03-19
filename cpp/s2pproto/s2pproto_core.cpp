//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <csignal>
#include <cstring>
#include <getopt.h>
#include <spdlog/spdlog.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/text_format.h>
#include "shared/shared_exceptions.h"
#include "buses/bus_factory.h"
#include "initiator/initiator_util.h"
#include "s2pproto_core.h"

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace spdlog;
using namespace s2p_util;
using namespace initiator_util;

void S2pProto::CleanUp() const
{
    if (bus) {
        bus->CleanUp();
    }
}

void S2pProto::TerminationHandler(int)
{
    instance->bus->SetRST(true);

    instance->CleanUp();

    // Process will terminate automatically
}

void S2pProto::Banner(bool header)
{
    if (header) {
        cout << "SCSI Target Emulator and SCSI Tools SCSI2Pi (Custom SCSI Command Execution Tool)\n"
            << "Version " << GetVersionString() << "\n"
            << "Copyright (C) 2023-2024 Uwe Seimet\n";
    }

    cout << "Usage: s2pproto [options]\n"
        << "  --scsi-target/-i ID:[LUN] SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                            default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID    Board (initiator) ID (0-7), default is 7.\n"
        << "  --log-level/-L LOG_LEVEL  Log level (trace|debug|info|warning|error|\n"
        << "                            critical|off),\n"
        << "                            default is 'info'.\n"
        << "  --input-file/-f FILE      Protobuf data input file,\n"
        << "                            by default in JSON format.\n"
        << "  --output-file/-F FILE     Protobuf data output file,\n"
        << "                            by default in JSON format.\n"
        << "  --binary-input            Input file has protobuf binary format.\n"
        << "  --binary-output           Generate protobuf binary format file.\n"
        << "  --text-input              Input file has protobuf text format.\n"
        << "  --text-output             Generate protobuf text format file.\n"
        << "  --version/-v              Display the program version.\n"
        << "  --help/-h                 Display this help.\n";
}

bool S2pProto::Init(bool in_process)
{
    bus = BusFactory::Instance().CreateBus(false, in_process);
    if (!bus) {
        return false;
    }

    executor = make_unique<S2pProtoExecutor>(*bus, initiator_id);

    instance = this;
    // Signal handler for cleaning up
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    return true;
}

bool S2pProto::ParseArguments(span<char*> args)
{
    const int OPT_BINARY_INPUT = 2;
    const int OPT_BINARY_OUTPUT = 3;
    const int OPT_TEXT_INPUT = 4;
    const int OPT_TEXT_OUTPUT = 5;

    const vector<option> options = {
        { "binary-input", no_argument, nullptr, OPT_BINARY_INPUT },
        { "binary-output", no_argument, nullptr, OPT_BINARY_OUTPUT },
        { "board-id", required_argument, nullptr, 'B' },
        { "help", no_argument, nullptr, 'h' },
        { "input-file", required_argument, nullptr, 'f' },
        { "output-file", required_argument, nullptr, 'F' },
        { "log-level", required_argument, nullptr, 'L' },
        { "scsi-target", required_argument, nullptr, 'i' },
        { "text-input", no_argument, nullptr, OPT_TEXT_INPUT },
        { "text-output", no_argument, nullptr, OPT_TEXT_OUTPUT },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string initiator = "7";
    string target;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "B:f:F:i:L:hnv",
        options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'B':
            initiator = optarg;
            break;

        case 'f':
            protobuf_input_filename = optarg;
            break;

        case 'F':
            protobuf_output_filename = optarg;
            break;

        case 'h':
            help = true;
            break;

        case 'i':
            target = optarg;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'v':
            version = true;
            break;

        case OPT_BINARY_INPUT:
            input_format = S2pProtoExecutor::protobuf_format::binary;
            break;

        case OPT_BINARY_OUTPUT:
            output_format = S2pProtoExecutor::protobuf_format::binary;
            break;

        case OPT_TEXT_INPUT:
            input_format = S2pProtoExecutor::protobuf_format::text;
            break;

        case OPT_TEXT_OUTPUT:
            output_format = S2pProtoExecutor::protobuf_format::text;
            break;

        default:
            Banner(false);
            return false;
        }
    }

    if (help) {
        Banner(true);
        return true;
    }

    if (version) {
        cout << GetVersionString() << '\n';
        return true;
    }

    if (!SetLogLevel(log_level)) {
        throw parser_exception("Invalid log level: '" + log_level + "'");
    }

    if (!GetAsUnsignedInt(initiator, initiator_id) || initiator_id > 7) {
        throw parser_exception("Invalid initiator ID: '" + initiator + "' (0-7)");
    }

    if (const string error = ProcessId(32, target, target_id, target_lun); !error.empty()) {
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

    if (protobuf_input_filename.empty()) {
        throw parser_exception("Missing input filename");
    }

    return true;
}

int S2pProto::Run(span<char*> args, bool in_process)
{
    if (args.size() < 2) {
        Banner(true);
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

    if (!in_process && !BusFactory::Instance().IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return EXIT_FAILURE;
    }

    executor->SetTarget(target_id, target_lun, false);

    int result = GenerateOutput(protobuf_input_filename, protobuf_output_filename);

    CleanUp();

    return result;
}

int S2pProto::GenerateOutput(const string &input_filename, const string &output_filename)
{
    PbResult result;
    if (string error = executor->Execute(input_filename, input_format, result); !error.empty()) {
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
    case S2pProtoExecutor::protobuf_format::binary: {
        ofstream out(output_filename, ios::binary);
        if (out.fail()) {
            cerr << "Error: " << "Can't open protobuf binary output file '" << output_filename << "'" << endl;
        }

        const string data = result.SerializeAsString();
        out.write(data.data(), data.size());
        break;
    }

    case S2pProtoExecutor::protobuf_format::json: {
        ofstream out(output_filename);
        if (out.fail()) {
            cerr << "Error: " << "Can't open protobuf JSON output file '" << output_filename << "'" << endl;
        }

        string json;
        (void)MessageToJsonString(result, &json);
        out << json << '\n';
        break;
    }

    case S2pProtoExecutor::protobuf_format::text: {
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
