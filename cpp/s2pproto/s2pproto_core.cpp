//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pproto_core.h"
#include <csignal>
#include <fstream>
#include <iostream>
#include <getopt.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include "buses/bus_factory.h"
#include "initiator/initiator_util.h"
#include "shared/s2p_exceptions.h"
#include "generated/target_api.pb.h"

using namespace google::protobuf;
using namespace google::protobuf::util;
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
        cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Custom SCSI Command Execution Tool)\n"
            << "Version " << GetVersionString() << "\n"
            << "Copyright (C) 2023-2025 Uwe Seimet\n";
    }

    cout << "Usage: " + APP_NAME + " [options]\n"
        << "  --scsi-target/-i ID:[LUN] SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                            default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID    Board (initiator) ID (0-7), default is 7.\n"
        << "  --log-level/-L LOG_LEVEL  Log level (trace|debug|info|warning|error|\n"
        << "                            critical|off), default is 'info'.\n"
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
    bus = bus_factory::CreateBus(false, in_process, APP_NAME, false);
    if (!bus) {
        return false;
    }

    executor = make_unique<S2pProtoExecutor>(*bus, initiator_id, *default_logger());

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
            input_format = ProtobufFormat::BINARY;
            break;

        case OPT_BINARY_OUTPUT:
            output_format = ProtobufFormat::BINARY;
            break;

        case OPT_TEXT_INPUT:
            input_format = ProtobufFormat::TEXT;
            break;

        case OPT_TEXT_OUTPUT:
            output_format = ProtobufFormat::TEXT;
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

    if (!SetLogLevel(*default_logger(), log_level)) {
        throw ParserException("Invalid log level: '" + log_level + "'");
    }

    if (initiator_id = ParseAsUnsignedInt(initiator); initiator_id < 0 || initiator_id > 7) {
        throw ParserException("Invalid initiator ID: '" + initiator + "' (0-7)");
    }

    if (const string &error = ParseIdAndLun(target, target_id, target_lun); !error.empty()) {
        throw ParserException(error);
    }

    if (target_id == -1) {
        throw ParserException("Missing target ID");
    }

    if (target_id == initiator_id) {
        throw ParserException("Target ID and initiator ID must not be identical");
    }

    if (target_lun == -1) {
        target_lun = 0;
    }

    if (protobuf_input_filename.empty()) {
        throw ParserException("Missing input filename");
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
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus\n";
        return EXIT_FAILURE;
    }

    if (!in_process && !bus->IsRaspberryPi()) {
        cerr << "Error: No RaSCSI/PiSCSI board found\n";
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
    if (const string &error = executor->Execute(input_filename, input_format, result); !error.empty()) {
        cerr << "Error: " << error << '\n';
        return EXIT_FAILURE;
    }

    if (output_filename.empty()) {
        string json;
        (void)MessageToJsonString(result, &json);
        cout << json << '\n';
        return EXIT_SUCCESS;
    }

    ofstream out(output_filename, output_format == ProtobufFormat::BINARY ? ios::binary : ios::out);
    if (!out) {
        cerr << "Error: Can't open protobuf data output file '" << output_filename << "'\n";
        return EXIT_FAILURE;
    }

    switch (output_format) {
    case ProtobufFormat::BINARY: {
        vector<uint8_t> data(result.ByteSizeLong());
        result.SerializeToArray(data.data(), data.size());
        out.write((const char*)data.data(), data.size());
        break;
    }

    case ProtobufFormat::JSON: {
        string json;
        (void)MessageToJsonString(result, &json);
        out << json << '\n';
        break;
    }

    case ProtobufFormat::TEXT: {
        string text;
        TextFormat::PrintToString(result, &text);
        out << text << '\n';
        break;
    }

    default:
        assert(false);
        break;
    }

    if (out.fail()) {
        cerr << "Error: Can't write protobuf data to output file '" << output_filename << "'\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
