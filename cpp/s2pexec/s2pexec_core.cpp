//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_core.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <getopt.h>
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"

using namespace filesystem;
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
    instance->CleanUp();

    // Process will terminate automatically
}

void S2pExec::Banner(bool header, bool usage)
{
    if (header) {
        cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (SCSI/SASI Command Execution Tool)\n"
            << "Version " << GetVersionString() << "\n"
            << "Copyright (C) 2023-2024 Uwe Seimet\n";
    }

    if (usage) {
        cout << "Usage: s2pexec [options]\n"
            << "  --scsi-target/-i ID:[LUN]     SCSI target device ID (0-7) and LUN (0-31),\n"
            << "                                default LUN is 0.\n"
            << "  --sasi-target/-h ID:[LUN]     SASI target device ID (0-7) and LUN (0-1),\n"
            << "                                default LUN is 0.\n"
            << "  --board-id/-B BOARD_ID        Board (initiator) ID (0-7), default is 7.\n"
            << "  --cdb/-c CDB[:CDB:...]        Command blocks to send in hexadecimal format.\n"
            << "  --data/-d DATA                Data to send with the command in hexadecimal\n"
            << "                                format. @ denotes a filename, e.g. @data.txt.\n"
            << "  --buffer-size/-b SIZE         Buffer size for received data,\n"
            << "                                default is 131072 bytes.\n"
            << "  --log-level/-L LOG_LEVEL      Log level (trace|debug|info|warning|error|\n"
            << "                                critical|off), default is 'info'.\n"
            << "  --binary-input-file/-f FILE   Binary input file with data to send.\n"
            << "  --binary-output-file/-F FILE  Binary output file for data received.\n"
            << "  --hex-output-file/-T FILE     Hexadecimal text output file for data received.\n"
            << "  --timeout/-o TIMEOUT          The command timeout in seconds, default is 3 s.\n"
            << "  --no-request-sense/-n         Do not run REQUEST SENSE on error.\n"
            << "  --reset-bus/-r                Reset the bus.\n"
            << "  --hex-only/-x                 Do not display/save the offset and ASCII data.\n"
            << "  --version/-v                  Display the program version.\n"
            << "  --help/-H                     Display this help.\n";
    }
}

bool S2pExec::Init(bool in_process)
{
    bus = BusFactory::Instance().CreateBus(false, in_process);
    if (!bus) {
        return false;
    }

    executor = make_unique<S2pExecExecutor>(*bus, initiator_id, *logger);

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

bool S2pExec::ParseArguments(span<char*> args)
{
    const vector<option> options = {
        { "buffer-size", required_argument, nullptr, 'b' },
        { "board-id", required_argument, nullptr, 'B' },
        { "binary-input-file", required_argument, nullptr, 'f' },
        { "binary-output-file", required_argument, nullptr, 'F' },
        { "cdb", required_argument, nullptr, 'c' },
        { "data", required_argument, nullptr, 'd' },
        { "help", no_argument, nullptr, 'H' },
        { "hex-only", no_argument, nullptr, 'x' },
        { "hex-output-file", required_argument, nullptr, 'T' },
        { "no-request-sense", no_argument, nullptr, 'n' },
        { "log-level", required_argument, nullptr, 'L' },
        { "reset-bus", no_argument, nullptr, 'r' },
        { "scsi-target", required_argument, nullptr, 'i' },
        { "sasi-target", required_argument, nullptr, 'h' },
        { "timeout", required_argument, nullptr, 'o' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string initiator;
    string target;
    string buf;
    string tout = "3";

    // Resetting these is important for the interactive mode
    command.clear();
    data.clear();
    request_sense = true;
    reset_bus = false;
    binary_input_filename.clear();
    hex_input_filename.clear();

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "b:B:c:d:f:F:h:i:o:L:T:Hnrvx",
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
            if (const string &d = optarg; d.starts_with('@') && d.size() > 1) {
                hex_input_filename = d.substr(1);
            }
            else {
                data = d;
            }
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

        case 'o':
            tout = optarg;
            break;

        case 'r':
            reset_bus = true;
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

        default:
            Banner(false, true);
            return false;
        }
    }

    if (help) {
        Banner(true, true);
        return true;
    }

    if (version) {
        cout << GetVersionString() << '\n';
        return true;
    }

    if (!SetLogLevel(*logger, log_level)) {
        throw parser_exception("Invalid log level: '" + log_level + "'");
    }

    if (!target.empty()) {
        if (const string &error = ProcessId(target, target_id, target_lun); !error.empty()) {
            throw parser_exception(error);
        }
    }

    // Most options only make sense when there is a command
    if (!command.empty()) {
        if (!initiator.empty() && (!GetAsUnsignedInt(initiator, initiator_id) || initiator_id > 7)) {
            throw parser_exception("Invalid initiator ID: '" + initiator + "' (0-7)");
        }

        if (target_id == -1 && !reset_bus) {
            throw parser_exception("Missing target ID");
        }

        if (target_id == initiator_id) {
            throw parser_exception("Target ID and initiator ID must not be identical");
        }

        if (target_lun == -1) {
            target_lun = 0;
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

        if ((!GetAsUnsignedInt(tout, timeout) || !timeout)) {
            throw parser_exception("Invalid command timeout value: '" + tout + "'");
        }
    }

    int buffer_size = DEFAULT_BUFFER_SIZE;
    if (!buf.empty() && (!GetAsUnsignedInt(buf, buffer_size) || !buffer_size)) {
        throw parser_exception("Invalid receive buffer size: '" + buf + "'");
    }
    buffer.resize(buffer_size);

    return true;
}

bool S2pExec::RunInteractive(bool in_process)
{
    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;
        return false;
    }

    if (!in_process && !bus->IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return false;
    }

    const string &prompt = "s2pexec";

    if (isatty(STDIN_FILENO)) {
        Banner(true, false);

        cout << "Entering interactive mode, Ctrl-D, \"exit\" or \"quit\" to quit\n";
    }

    while (true) {
        string input = GetLine(prompt);
        if (input.empty()) {
            break;
        }

        // Like with bash "!!" repeats the last command
        if (input == "!!") {
            input = last_input;
            cout << input << '\n';
        }
        else if (!input.starts_with('-')) {
            cerr << "Error: Missing command" << endl;
            continue;
        } else {
            last_input = input;
        }

        const auto &args = Split(input, ' ');

        vector<char*> interactive_args;
        interactive_args.emplace_back(strdup(prompt.c_str()));
        interactive_args.emplace_back(strdup(args[0].c_str()));
        for (size_t i = 1; i < args.size(); i++) {
            if (!args[i].empty()) {
                interactive_args.emplace_back(strdup(args[i].c_str()));
            }
        }

        try {
            if (!ParseArguments(interactive_args)) {
                continue;
            }
        }
        catch (const parser_exception &e) {
            cerr << "Error: " << e.what() << endl;
            continue;
        }

        if (!command.empty() || reset_bus) {
            Run();
        }
    }

    CleanUp();

    return true;
}

int S2pExec::Run(span<char*> args, bool in_process)
{
    if (args.size() < 2 || in_process) {
        return RunInteractive(in_process) ? EXIT_SUCCESS : -1;
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

    if (command.empty() && !reset_bus) {
        cerr << "Error: Missing command" << endl;
        return -1;
    }

    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;
        return -1;
    }

    if (!in_process && !bus->IsRaspberryPi()) {
        cerr << "Error: No board hardware support" << endl;
        return -1;
    }

    const int status = Run();

    CleanUp();

    return status;
}

int S2pExec::Run()
{
    executor->SetTarget(target_id, target_lun, sasi);

    if (reset_bus) {
        ResetBus(*bus);
        return EXIT_SUCCESS;
    }

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

    return result;
}

tuple<sense_key, asc, int> S2pExec::ExecuteCommand()
{
    vector<byte> cmd_bytes;

    try {
        cmd_bytes = HexToBytes(command);
    }
    catch (const out_of_range&)
    {
        throw execution_exception("Invalid CDB input format: '" + command + "'");
    }

    vector<uint8_t> cdb;
    ranges::transform(cmd_bytes, back_inserter(cdb), [](const byte b) {return static_cast<uint8_t>(b) & 0xff;});

    if (!data.empty()) {
        if (const string &error = ConvertData(data); !error.empty()) {
            throw execution_exception(error);
        }
    }
    else if (!binary_input_filename.empty() || !hex_input_filename.empty()) {
        if (const string &error = ReadData(); !error.empty()) {
            throw execution_exception(error);
        }
    }

    const int status = executor->ExecuteCommand(cdb, buffer, timeout);
    if (status) {
        if (status != 0xff && request_sense) {
            return executor->GetSenseData();
        }
        else {
            const string_view &command_name = CommandMetaData::Instance().GetCommandName(
                static_cast<scsi_command>(cdb[0]));
            throw execution_exception(
                fmt::format("Can't execute command {}",
                    !command_name.empty() ?
                        fmt::format("{0} (${1:02x})", command_name, cdb[0]) : fmt::format("${:02x}", cdb[0])));
        }
    }

    if (data.empty() && binary_input_filename.empty() && hex_input_filename.empty()) {
        if (const int count = executor->GetByteCount(); count) {
            logger->debug("Initiator received {} data byte(s)", count);

            if (const string &error = WriteData(span<const uint8_t>(buffer.begin(), buffer.begin() + count)); !error.empty()) {
                throw execution_exception(error);
            }
        }

        // Do not re-use input files
        binary_input_filename.clear();
        hex_input_filename.clear();
    }

    return {sense_key {0}, asc {0}, 0};
}

string S2pExec::ReadData()
{
    const string &filename = binary_input_filename.empty() ? hex_input_filename : binary_input_filename;
    const bool text = binary_input_filename.empty();

    ifstream in(filename, text ? ios::in : ios::in | ios::binary);
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

string S2pExec::WriteData(span<const uint8_t> data)
{
    const string &filename = binary_output_filename.empty() ? hex_output_filename : binary_output_filename;
    const bool text = binary_output_filename.empty();

    string hex = FormatBytes(data, static_cast<int>(data.size()), 0, hex_only);

    if (filename.empty()) {
        cout << hex << '\n';
    }
    else {
        ofstream out(filename, text ? ios::out : ios::out | ios::binary);
        if (out.fail()) {
            return fmt::format("Can't open output file '{0}': {1}", filename, strerror(errno));
        }

        hex += "\n";
        out.write(text ? hex.data() : (const char*)data.data(), hex.size());
        if (out.fail()) {
            return fmt::format("Can't write to file '{0}': {1}", filename, strerror(errno));
        }
    }

    return "";
}

string S2pExec::ConvertData(const string &hex)
{
    vector<byte> bytes;
    try {
        bytes = HexToBytes(hex);
    }
    catch (const out_of_range&) {
        return "Invalid data input format";
    }

    buffer.clear();
    ranges::transform(bytes, back_inserter(buffer), [](const byte b) {return static_cast<uint8_t>(b);});

    return "";
}
