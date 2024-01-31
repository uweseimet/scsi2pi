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
#include <spdlog/spdlog.h>
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "initiator/initiator_util.h"
#include "s2pexec_core.h"

using namespace filesystem;
using namespace spdlog;
using namespace s2p_util;
using namespace initiator_util;

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
        << "  --scsi-target/-i ID:[LUN]     SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                                default LUN is 0.\n"
        << "  --sasi-target/-h ID:[LUN]     SASI target device ID (0-7) and LUN (0-1),\n"
        << "                                default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID        Board (initiator) ID (0-7), default is 7.\n"
        << "  --cdb/-c CDB                  Command to send in hexadecimal format.\n"
        << "  --data/-d DATA                Data to send with the command in\n"
        << "                                hexadecimal format.\n"
        << "  --buffer-size/-b SIZE         Buffer size for receiving data.\n"
        << "  --log-level/-L LOG_LEVEL      Log level (trace|debug|info|warning|error\n"
        << "                                |off), default is 'info'.\n"
        << "  --binary-input-file/-f FILE   Binary input file with data to send.\n"
        << "  --binary-output-file/-F FILE  Binary output file for data received.\n"
        << "  --hex-input-file/-t FILE      Hexadecimal text input file with data to send.\n"
        << "  --hex-input-file/-T FILE      Hexadecimal text output file for data received.\n"
        << "  --timeout TIMEOUT             The command timeout in seconds, default is 3 s.\n"
        << "  --no-request-sense            Do not run REQUEST SENSE on error.\n"
        << "  --hex-only/-x                 Do not display/save the offset and ASCI data.\n"
        << "  --version/-v                  Display the s2pexec version.\n"
        << "  --help/-H                     Display this help.\n";
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
        executor = make_unique<S2pExecExecutor>(*bus, initiator_id);
    }

    return bus != nullptr;
}

bool S2pExec::ParseArguments(span<char*> args)
{
    const int OPT_TIMEOUT = 2;

    const vector<option> options = {
        { "buffer-size", required_argument, nullptr, 'b' },
        { "board-id", required_argument, nullptr, 'B' },
        { "binary-input-file", required_argument, nullptr, 'f' },
        { "binary-output-file", required_argument, nullptr, 'F' },
        { "cdb", required_argument, nullptr, 'c' },
        { "data", required_argument, nullptr, 'd' },
        { "help", no_argument, nullptr, 'H' },
        { "hex-input-file", required_argument, nullptr, 't' },
        { "hex-only", no_argument, nullptr, 'x' },
        { "hex-output-file", required_argument, nullptr, 'T' },
        { "no-request-sense", no_argument, nullptr, 'n' },
        { "log-level", required_argument, nullptr, 'L' },
        { "scsi-target", required_argument, nullptr, 'i' },
        { "sasi-target", required_argument, nullptr, 'h' },
        { "timeout", required_argument, nullptr, OPT_TIMEOUT },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string initiator = "7";
    string target;
    string buf;
    string tout = "3";

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "b:B:c:d:f:F:h:i:L:t:T:Hnvx",
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

        case 'd':
            data = optarg;
            break;

        case 'f':
            binary_input_filename = optarg;
            break;

        case 'F':
            binary_output_filename = optarg;
            break;

        case 'h':
            target = optarg;
            sasi = true;
            break;

        case 'H':
            help = true;
            break;

        case 'i':
            target = optarg;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'n':
            request_sense = false;
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

        case 'x':
            hex_only = true;
            break;

        case OPT_TIMEOUT:
            tout = optarg;
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

    if (command.empty()) {
        throw parser_exception("Missing command block");
    }

    if (!data.empty() && (!binary_input_filename.empty() || !hex_input_filename.empty())) {
        throw parser_exception("An input file is not permitted when providing explicit data");
    }

    if (!binary_input_filename.empty() && !hex_input_filename.empty()) {
        throw parser_exception("There can only be a single input file");
    }

    if (!binary_output_filename.empty() && !hex_output_filename.empty()) {
        throw parser_exception("There can only be a single output file");
    }

    if (!GetAsUnsignedInt(tout, timeout) || !timeout) {
        throw parser_exception("Invalid command timeout value: '" + tout + "'");
    }

    int buffer_size = DEFAULT_BUFFER_SIZE;
    if (!buf.empty() && (!GetAsUnsignedInt(buf, buffer_size) || !buffer_size)) {
        throw parser_exception("Invalid receive buffer size: '" + buf + "'");
    }
    buffer.resize(buffer_size);

    return true;
}

int S2pExec::Run(span<char*> args, bool in_process)
{
    if (args.size() < 2) {
        Banner(true);
        return -1;
    }

    try {
        if (!ParseArguments(args)) {
            return -1;
        }
        else if (version || help) {
            return EXIT_SUCCESS;
        }
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }

    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;
        return -1;
    }

    if (!in_process && !bus_factory->IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return -1;
    }

    executor->SetTarget(target_id, target_lun, sasi);

    int result = EXIT_SUCCESS;
    try {
        const auto [sense_key, asc, ascq] = ExecuteCommand();
        if (sense_key != sense_key::no_sense || asc != asc::no_additional_sense_information || ascq) {
            if (static_cast<int>(sense_key) != -1) {
                cerr << "Error: " << FormatSenseData(sense_key, asc, ascq) << endl;

                result = static_cast<int>(asc);
            }
            else {
                result = -1;
            }
        }
    }
    catch (const execution_exception &e) {
        cerr << "Error: " << e.what() << endl;
        result = -1;
    }

    CleanUp();

    return result;
}

tuple<sense_key, asc, int> S2pExec::ExecuteCommand()
{
    vector<byte> cmd_bytes;

    try {
        cmd_bytes = HexToBytes(command);
    }
    catch (const parser_exception&)
    {
        throw execution_exception("Invalid CDB input format: '" + command + "'");
    }

    vector<uint8_t> cdb;
    for (byte b : cmd_bytes) {
        cdb.emplace_back(static_cast<uint8_t>(b) & 0xff);
    }

    if (!data.empty()) {
        if (const string &error = ConvertData(data); !error.empty()) {
            throw execution_exception(error);
        }
        debug("Sending {} data bytes", buffer.size());
    }
    else if (!binary_input_filename.empty() || !hex_input_filename.empty()) {
        if (const string &error = ReadData(); !error.empty()) {
            throw execution_exception(error);
        }
        debug("Sending {} data bytes", buffer.size());
    }

    const int status = executor->ExecuteCommand(static_cast<scsi_command>(cdb[0]), cdb, buffer, timeout);
    if (status) {
        if (status != 0xff && request_sense) {
            return executor->GetSenseData();
        }
        else {
            throw execution_exception(fmt::format("Can't execute command ${:02x}", cdb[0]));
        }
    }

    if (data.empty() && binary_input_filename.empty() && hex_input_filename.empty()) {
        const int count = executor->GetByteCount();

        debug("Received {} data bytes", count);

        if (count) {
            if (const string &error = WriteData(count); !error.empty()) {
                throw execution_exception(error);
            }
        }
    }

    return {sense_key {0}, asc {0}, 0};
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
            if (const string &error = ConvertData(ss.str()); !error.empty()) {
                return error;
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

string S2pExec::ConvertData(const string &data)
{
    vector<byte> bytes;
    try {
        bytes = HexToBytes(data);
    }
    catch (const parser_exception&) {
        return "Invalid data input format";
    }

    buffer.clear();
    for (const byte b : bytes) {
        buffer.emplace_back(static_cast<uint8_t>(b));
    }

    return "";
}
