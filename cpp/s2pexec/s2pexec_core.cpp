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

void S2pExec::Banner(bool header)
{
    if (header) {
        cout << s2p_util::Banner("(SCSI/SASI Command Execution Tool)");
    }

    cout << "Usage: s2pexec [options]\n"
        << "  --scsi-target/-i ID:[LUN]       SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                                  default LUN is 0.\n"
        << "  --sasi-target/-h ID:[LUN]       SASI target device ID (0-7) and LUN (0-1),\n"
        << "                                  default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID          Board (initiator) ID (0-7), default is 7.\n"
        << "  --cdb/-c CDB                    SCSI command to send in hexadecimal format.\n"
        << "  --buffer-size/-b SIZE           Buffer size for receiving data.\n"
        << "  --log-level/-L LOG_LEVEL        Log level (trace|debug|info|warning|error\n"
        << "                                  |off), default is 'info'.\n"
        << "  --binary-input-file/-f FILE     Optional binary input file with data to send.\n"
        << "  --binary-output-file/-F FILE    Optional binary output file for data received.\n"
        << "  --hex-input-file/-t FILE        Optional text input file with data to send.\n"
        << "  --hex-input-file/-T FILE        Optional text output file for data received.\n"
        << "  --protobuf-input-file/-p FILE   Protobuf data input file,\n"
        << "                                  by default in JSON format.\n"
        << "  --protobuf-output-file/-P FILE  Protobuf data output file,\n"
        << "                                  by default in JSON format.\n"
        << "  --binary-protobuf-input         Input file has protobuf binary format.\n"
        << "  --binary-protobuf-output        Generate protobuf binary format file.\n"
        << "  --text-protobuf-input           Input file has protobuf tet format.\n"
        << "  --text-protobuf-output          Generate protobuf text format file.\n"
        << "  --no-request-sense              Do not run REQUEST SENSE on error.\n"
        << "  --hex-only                      Do not display/save the offset and ASCI data.\n"
        << "  --version/-v                    Display s2pexec version.\n"
        << "  --help/-H                       Display this help.\n";
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
    const int OPT_BINARY_PROTOBUF_INPUT = 2;
    const int OPT_BINARY_PROTOBUF_OUTPUT = 3;
    const int OPT_TEXT_PROTOBUF_INPUT = 4;
    const int OPT_TEXT_PROTOBUF_OUTPUT = 5;
    const int OPT_HEX_ONLY = 6;

    const vector<option> options = {
        { "binary-protobuf-input", no_argument, nullptr, OPT_BINARY_PROTOBUF_INPUT },
        { "binary-protobuf-output", no_argument, nullptr, OPT_BINARY_PROTOBUF_OUTPUT },
        { "buffer-size", required_argument, nullptr, 'b' },
        { "board-id", required_argument, nullptr, 'B' },
        { "cdb", required_argument, nullptr, 'c' },
        { "binary-input-file", required_argument, nullptr, 'f' },
        { "binary-output-file", required_argument, nullptr, 'F' },
        { "help", no_argument, nullptr, 'H' },
        { "hex-input-file", required_argument, nullptr, 't' },
        { "hex-only", no_argument, nullptr, OPT_HEX_ONLY },
        { "hex-output-file", required_argument, nullptr, 'T' },
        { "no-request-sense", no_argument, nullptr, 'n' },
        { "protobuf-input-file", required_argument, nullptr, 'p' },
        { "protobuf-output-file", required_argument, nullptr, 'P' },
        { "log-level", required_argument, nullptr, 'L' },
        { "scsi-target", required_argument, nullptr, 'i' },
        { "sasi-target", required_argument, nullptr, 'h' },
        { "text-input", no_argument, nullptr, OPT_TEXT_PROTOBUF_INPUT },
        { "text-output", no_argument, nullptr, OPT_TEXT_PROTOBUF_OUTPUT },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string initiator = "7";
    string target;
    string buf;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "b:c:f:f:F:h:i:L:p:P:t:T:nvBH",
        options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'b':
            buf = optarg;
            break;

        case 'B':
            initiator = optarg;
            break;

        case 'c':
            command = optarg;
            break;

        case 'f':
            binary_input_filename = optarg;
            break;

        case 'F':
            binary_output_filename = optarg;
            break;

        case 'n':
            request_sense = false;
            break;

        case 'p':
            protobuf_input_filename = optarg;
            break;

        case 'H':
            help = true;
            break;

        case 'h':
            target = optarg;
            sasi = true;
            break;

        case 'i':
            target = optarg;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'P':
            protobuf_output_filename = optarg;
            break;

        case 't':
            hex_input_filename = optarg;
            break;

        case 'T':
            hex_output_filename = optarg;
            break;

        case 'v':
            version = true;
            break;

        case OPT_BINARY_PROTOBUF_INPUT:
            input_format = S2pExecExecutor::protobuf_format::binary;
            break;

        case OPT_BINARY_PROTOBUF_OUTPUT:
            output_format = S2pExecExecutor::protobuf_format::binary;
            break;

        case OPT_TEXT_PROTOBUF_INPUT:
            input_format = S2pExecExecutor::protobuf_format::text;
            break;

        case OPT_TEXT_PROTOBUF_OUTPUT:
            output_format = S2pExecExecutor::protobuf_format::text;
            break;

        case OPT_HEX_ONLY:
            hex_only = true;
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

    if (!SetLogLevel()) {
        throw parser_exception("Invalid log level: '" + log_level + "'");
    }

    if (!GetAsUnsignedInt(initiator, initiator_id) || initiator_id > 7) {
        throw parser_exception("Invalid initiator ID: '" + initiator + "' (0-7)");
    }

    if (const string error = ProcessId(8, sasi ? 2 : 32, target, target_id, target_lun); !error.empty()) {
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

    if (!command.empty()) {
        if (!binary_input_filename.empty() && !hex_input_filename.empty()) {
            throw parser_exception("There can only be a single input file");
        }

        if (!binary_output_filename.empty() && !hex_output_filename.empty()) {
            throw parser_exception("There can only be a single output file");
        }

        int buffer_size = DEFAULT_BUFFER_SIZE;
        if (!buf.empty() && (!GetAsUnsignedInt(buf, buffer_size) || !buffer_size)) {
            throw parser_exception("Invalid receive buffer size: '" + buf + "'");
        }
        buffer.resize(buffer_size);
    }
    else if (protobuf_input_filename.empty()) {
        throw parser_exception("Missing input filename");
    }

    return true;
}

int S2pExec::Run(span<char*> args, bool in_process)
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

    if (!in_process && !bus_factory->IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return EXIT_FAILURE;
    }

    scsi_executor->SetTarget(target_id, target_lun);

    int result = EXIT_SUCCESS;
    if (!command.empty()) {
        if (const string error = ExecuteCommand(); !error.empty()) {
            cerr << "Error: " << error << endl;
            result = EXIT_FAILURE;
        }
    }
    else {
        result = GenerateOutput(input_format, protobuf_input_filename, output_format, protobuf_output_filename);
    }

    CleanUp();

    return result;
}

string S2pExec::ExecuteCommand()
{
    vector<byte> cmd_bytes;

    try {
        cmd_bytes = HexToBytes(command);
    }
    catch (const parser_exception&)
    {
        return "Error: Invalid CDB input format: '" + command + "'";
    }

    vector<uint8_t> cdb;
    for (byte b : cmd_bytes) {
        cdb.emplace_back(static_cast<uint8_t>(b) & 0xff);
    }

    // Only send data if there is a data file
    if (!binary_input_filename.empty() || !hex_input_filename.empty()) {
        if (const string &error = ReadData(); !error.empty()) {
            return error;
        }

        debug(fmt::format("Sending {} data bytes", buffer.size()));
    }

    const bool status = scsi_executor->ExecuteCommand(static_cast<scsi_command>(cdb[0]), cdb, buffer, sasi);
    if (!status) {
        if (request_sense) {
            warn("Device reported an error, running REQUEST SENSE");
            return scsi_executor->GetSenseData(sasi);
        }
        else {
            warn("Device reported an error");
            return "";
        }
    }

    if (binary_input_filename.empty() && hex_input_filename.empty()) {
        const int count = scsi_executor->GetByteCount();

        debug(fmt::format("Received {} data bytes", count));

        if (count) {
            if (const string &error = WriteData(count); !error.empty()) {
                return error;
            }
        }
    }

    return "";
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

string S2pExec::ReadData()
{
    const string &filename = binary_input_filename.empty() ? hex_input_filename : binary_input_filename;
    const bool text = binary_input_filename.empty();

    fstream in(filename, text ? ios::in : ios::in | ios::binary);
    if (in.fail()) {
        return fmt::format("Can't open input file '{0}': {1}", filename, strerror(errno));
    }

    if (text) {
        stringstream ss;
        ss << in.rdbuf();
        if (!in.fail()) {
            vector<byte> bytes;
            try {
                bytes = HexToBytes(ss.str());
            }
            catch (const parser_exception&) {
                return "Invalid data input format";
            }

            buffer.clear();
            for (const byte b : bytes) {
                buffer.emplace_back(static_cast<uint8_t>(b));
            }
        }
    }
    else {
        const size_t size = file_size(path(filename));
        buffer.resize(size);
        in.read((char*)buffer.data(), size);
    }

    return in.fail() ? fmt::format("Can't read from file '{0}': {1}", filename, strerror(errno)) : "";
}

string S2pExec::WriteData(int count)
{
    const string &filename = binary_output_filename.empty() ? hex_output_filename : binary_output_filename;
    const bool text = binary_output_filename.empty();

    string hex = FormatBytes(buffer, count, hex_only);

    if (filename.empty()) {
        if (count) {
            cout << hex << '\n';
        }
    }
    else {
        fstream out(filename, text ? ios::out : ios::out | ios::binary);
        if (out.fail()) {
            return fmt::format("Can't open output file '{0}': {1}", filename, strerror(errno));
        }

        if (count) {
            hex += "\n";
            out.write(text ? hex.data() : (const char*)buffer.data(), hex.size());
            if (out.fail()) {
                return fmt::format("Can't write to file '{0}': {1}", filename, strerror(errno));
            }
        }
    }

    return "";
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

