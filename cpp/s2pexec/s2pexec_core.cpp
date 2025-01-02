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
    if (executor) {
        executor->CleanUp();
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
        cout << "Usage: " + APP_NAME + " [options]\n"
            << "  --scsi-target/-i ID:[LUN]      SCSI target device ID (0-7) and LUN (0-31),\n"
            << "                                 default LUN is 0.\n"
            << "  --sasi-target/-h ID:[LUN]      SASI target device ID (0-7) and LUN (0-1),\n"
            << "                                 default LUN is 0.\n"
            << "  --board-id/-B BOARD_ID         Board (initiator) ID (0-7), default is 7.\n"
            << "  --cdb/-c CDB[:CDB:...]         Command blocks to send in hexadecimal format.\n"
            << "  --data/-d DATA                 Data to send with the command in hexadecimal\n"
            << "                                 format. @ denotes a filename, e.g. @data.txt.\n"
            << "  --buffer-size/-b SIZE          Buffer size for received data,\n"
            << "                                 default is 131072 bytes.\n"
            << "  --log-level/-L LEVEL           Log level (trace|debug|info|warning|error|\n"
            << "                                 critical|off), default is 'info'.\n"
            << "  --log-limit/-l LIMIT           The number of data bytes being logged,\n"
            << "                                 0 means no limit. Default is 128.\n"
            << "  --binary-input-file/-f FILE    Binary input file with data to send.\n"
            << "  --binary-output-file/-F FILE   Binary output file for data received.\n"
            << "  --hex-output-file/-T FILE      Hexadecimal text output file for data received.\n"
            << "  --timeout/-o TIMEOUT           The command timeout in seconds, default is 3 s.\n"
            << "  --request-sense/-R             Automatically send REQUEST SENSE on error.\n"
            << "  --reset-bus/-r                 Reset the bus.\n"
            << "  --hex-only/-x                  Do not display/save the offset and ASCII data.\n"
            << "  --scsi-generic/-g DEVICE_FILE  Use the Linux SG driver instead of a\n"
            << "                                 RaSCSI/PiSCSI board.\n"
            << "  --version/-v                   Display the program version.\n"
            << "  --help/-H                      Display this help.\n";
    }
}

bool S2pExec::Init(bool in_process)
{
    if (!executor) {
        executor = make_unique<S2pExecExecutor>(*s2pexec_logger);
    }

    if (!use_sg) {
        if (const string &error = executor->Init(initiator_id, APP_NAME, in_process); !error.empty()) {
            cerr << "Error: " << error << '\n';
            return false;
        }

        instance = this;
        // Signal handler for cleaning up
        struct sigaction termination_handler;
        termination_handler.sa_handler = TerminationHandler;
        sigemptyset(&termination_handler.sa_mask);
        termination_handler.sa_flags = 0;
        sigaction(SIGINT, &termination_handler, nullptr);
        sigaction(SIGTERM, &termination_handler, nullptr);
        signal(SIGPIPE, SIG_IGN);
    }
    else if (const string &error = executor->Init(device_file); !error.empty()) {
        cerr << "Error: " << error << '\n';
        return false;
    }

    return true;
}

bool S2pExec::ParseArguments(span<char*> args, bool in_process)
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
        { "request-sense", no_argument, nullptr, 'R' },
        { "log-level", required_argument, nullptr, 'L' },
        { "log-limit/-l", required_argument, nullptr, 'l' },
        { "reset-bus", no_argument, nullptr, 'r' },
        { "scsi-generic", required_argument, nullptr, 'g' },
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
    request_sense = false;
    reset_bus = false;
    binary_input_filename.clear();
    hex_input_filename.clear();

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "b:B:c:d:f:F:g:h:i:o:L:l:T:HrRvx",
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

        case 'g':
            if (device_file != optarg) {
                target = "";
                device_file = optarg;
                use_sg = true;
                is_initialized = false;
            }
            break;

        case 'h':
            target = optarg;
            sasi = true;
            break;

        case 'H':
            help = true;
            break;

        case 'i':
            if (target != optarg) {
                device_file = "";
                target = optarg;
                use_sg = false;
                is_initialized = false;
            }
            break;

        case 'l':
            log_limit = optarg;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'o':
            tout = optarg;
            break;

        case 'r':
            reset_bus = true;
            break;

        case 'R':
            request_sense = true;
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

    if (!SetLogLevel(*s2pexec_logger, log_level)) {
        const string l = log_level;
        log_level.clear();
        throw ParserException("Invalid log level: '" + l + "'");
    }

    if (!initiator.empty()) {
        if (initiator_id = ParseAsUnsignedInt(initiator); initiator_id == -1 || initiator_id > 7) {
            throw ParserException("Invalid initiator ID: '" + initiator + "' (0-7)");
        }
    }

    if (!target.empty()) {
        if (const string &error = ParseIdAndLun(target, target_id, target_lun); !error.empty()) {
            throw ParserException(error);
        }
    }

    if (executor && executor->IsSg() != use_sg) {
        executor->CleanUp();
    }

    if (!is_initialized && (!device_file.empty() || !target.empty())) {
        is_initialized = Init(in_process);
        if (!is_initialized) {
            return false;
        }
    }

    if (!log_limit.empty()) {
        if (const int limit = ParseAsUnsignedInt(log_limit); limit < 0) {
            const string l = log_limit;
            log_limit.clear();
            throw ParserException("Invalid log limit: '" + l + "'");
        }
        else {
            formatter.SetLimit(limit);
            if (executor) {
                executor->SetLimit(limit);
            }
        }
    }

    if (target_id == initiator_id) {
        throw ParserException("Target ID and initiator ID must not be identical");
    }

    if (target_lun == -1) {
        target_lun = 0;
    }

    if (executor) {
        executor->SetTarget(target_id, target_lun, sasi);
    }

    // Some options only make sense when there is a command
    if (!command.empty()) {
        if (!use_sg && target_id == -1 && !reset_bus) {
            throw ParserException("Missing target ID");
        }

        if (!data.empty() && (!binary_input_filename.empty() || !hex_input_filename.empty())) {
            throw ParserException("An input file is not permitted when providing explicit data");
        }

        if (!binary_input_filename.empty() && !hex_input_filename.empty()) {
            throw ParserException("There can only be a single input file");
        }

        if (!binary_output_filename.empty() && !hex_output_filename.empty()) {
            throw ParserException("There can only be a single output file");
        }

        if (timeout = ParseAsUnsignedInt(tout); timeout <= 0) {
            throw ParserException("Invalid command timeout value: '" + tout + "'");
        }
    }

    int buffer_size = DEFAULT_BUFFER_SIZE;
    if (!buf.empty()) {
        if (buffer_size = ParseAsUnsignedInt(buf); buffer_size <= 0) {
            throw ParserException("Invalid receive buffer size: '" + buf + "'");
        }
    }
    buffer.resize(buffer_size);

    return true;
}

void S2pExec::RunInteractive(bool in_process)
{
    if (isatty(STDIN_FILENO)) {
        Banner(true, false);

        cout << "Entering interactive mode, Ctrl-D, \"exit\" or \"quit\" to quit\n";
    }

    while (true) {
        string input = GetLine(APP_NAME);
        if (input.empty()) {
            break;
        }

        // Like with bash "!!" repeats the last command
        if (input == "!!") {
            input = last_input;
            cout << input << '\n';
        }
        else if (!input.starts_with('-')) {
            cerr << "Error: Missing command\n";
            continue;
        } else {
            last_input = input;
        }

        const auto &args = Split(input, ' ');

        vector<char*> interactive_args;
        interactive_args.emplace_back(strdup(APP_NAME.c_str()));
        interactive_args.emplace_back(strdup(args[0].c_str()));
        for (size_t i = 1; i < args.size(); i++) {
            if (!args[i].empty()) {
                interactive_args.emplace_back(strdup(args[i].c_str()));
            }
        }

        try {
            if (!ParseArguments(interactive_args, in_process)) {
                continue;
            }
        }
        catch (const ParserException &e) {
            cerr << "Error: " << e.what() << '\n';
            continue;
        }

        if (!command.empty() || (executor && reset_bus)) {
            Run();
        }
    }

    CleanUp();
}

int S2pExec::Run(span<char*> args, bool in_process)
{
    s2pexec_logger = CreateLogger(APP_NAME);

    if (args.size() < 2 || in_process) {
        RunInteractive(in_process);
        return EXIT_SUCCESS;
    }

    try {
        if (!ParseArguments(args, in_process)) {
            return -1;
        }
        else if (version || help) {
            return EXIT_SUCCESS;
        }
    }
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
        return -1;
    }

    if (command.empty() && !reset_bus) {
        cerr << "Error: Missing command\n";
        return -1;
    }

    const int status = Run();

    CleanUp();

    return status;
}

int S2pExec::Run()
{
    if (reset_bus && executor) {
        executor->ResetBus();
        return EXIT_SUCCESS;
    }

    int result = EXIT_SUCCESS;
    try {
        const auto [sense_key, asc, ascq] = ExecuteCommand();
        if (sense_key != SenseKey::NO_SENSE || asc != Asc::NO_ADDITIONAL_SENSE_INFORMATION || ascq) {
            if (static_cast<int>(sense_key) != -1) {
                cerr << "Error: " << FormatSenseData(sense_key, asc, ascq) << '\n';

                result = static_cast<int>(asc);
            }
            else {
                result = -1;
            }
        }
    }
    catch (const execution_exception &e) {
        cerr << "Error: " << e.what() << '\n';
        result = -1;
    }

    return result;
}

tuple<SenseKey, Asc, int> S2pExec::ExecuteCommand()
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

    const int status = executor->ExecuteCommand(cdb, buffer, timeout, true);
    if (status) {
        if (status != 0xff) {
            if (request_sense) {
                return executor->GetSenseData();
            }
        }
        else {
            throw execution_exception(fmt::format("Can't execute command {}",
                CommandMetaData::Instance().GetCommandName(static_cast<ScsiCommand>(cdb[0]))));
        }
    }

    if (cdb[0] == static_cast<uint8_t>(ScsiCommand::REQUEST_SENSE)) {
        vector<byte> sense_data;
        transform(buffer.begin(), buffer.begin() + 18, back_inserter(sense_data),
            [](const uint8_t d) {return static_cast<byte>(d);});
        s2pexec_logger->debug(FormatSenseData(sense_data));
    }

    if (data.empty() && binary_input_filename.empty() && hex_input_filename.empty()) {
        if (const int count = executor->GetByteCount(); count) {
            s2pexec_logger->debug("Received {} data byte(s)", count);
            if (const string &error = WriteData(span<const uint8_t>(buffer.begin(), buffer.begin() + count)); !error.empty()) {
                throw execution_exception(error);
            }
        }

        // Do not re-use input files
        binary_input_filename.clear();
        hex_input_filename.clear();
    }

    return {SenseKey {0}, Asc {0}, 0};
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
        const size_t size = file_size(filename);
        buffer.resize(size);
        in.read((char*)buffer.data(), size);
    }

    return in.fail() ? fmt::format("Can't read from file '{0}': {1}", filename, strerror(errno)) : "";
}

string S2pExec::WriteData(span<const uint8_t> data)
{
    const string &filename = binary_output_filename.empty() ? hex_output_filename : binary_output_filename;
    const bool text = binary_output_filename.empty();

    string hex = formatter.FormatBytes(data, data.size(), hex_only);

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
