//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_core.h"
#include <fstream>
#include <iostream>
#include <locale>
#include <getopt.h>
#include <unistd.h>
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_version.h"
#include "s2pctl_commands.h"

using namespace s2p_interface_util;
using namespace s2p_util;

void S2pCtl::Banner(bool usage) const
{
    cout << s2p_util::Banner("(Server Controller Tool)");

    if (usage) {
        cout << "Usage: s2pctl [options]\n"
            << "  --binary-protobuf FILENAME     Do not send command to s2p\n"
            << "                                 but write it to a protobuf binary file.\n"
            << "  --block-size/-b BLOCK_SIZE     Optional default block size, a multiple of 4.\n"
            << "  --caching-mode/-m MODE         Caching mode (piscsi|write-through|linux\n"
            << "                                 |linux-optimized), default is PiSCSI\n"
            << "                                 compatible caching.\n"
            << "  --command/-c COMMAND           Command (attach|detach|insert|eject|protect\n"
            << "                                 |unprotect).\n"
            << "  --copy/-x CURRENT:NEW          Copy an image file.\n"
            << "  --create/-C FILENAME:SIZE      Create an empty image file.\n"
            << "  --delete/-d FILENAME           Delete an image file.\n"
            << "  --detach-all/-D                Detach all devices.\n"
            << "  --file/-f FILE|PARAMS          Image file path or device-specific parameters.\n"
            << "  --help/-h                      Display this help.\n"
            << "  --host/-H HOST                 s2p host to connect to, default is 'localhost'.\n"
            << "  --id/-i ID[:LUN]               Target device ID (0-7) and LUN\n"
            << "                                 (SCSI: 0-31, SASI: 0-1), default LUN is 0.\n"
            << "  --image-folder/-F FOLDER       Default location for image files,\n"
            << "                                 default is '~/images'.\n"
            << "  --json-protobuf FILENAME       Do not send command to s2p\n"
            << "                                 but write it to a protobuf JSON file.\n"
            << "  --list-devices/-l              Display device list.\n"
            << "  --list-device-types/-T         List available device types.\n"
            << "  --list-extensions              List supported file extensions\n"
            << "                                 and the device types they map to.\n"
            << "  --list-images/-e               List images files in the default image folder.\n"
            << "  --list-image-info/-E FILENAME  Display image file information.\n"
            << "  --list-interfaces/-N           List network interfaces that are up.\n"
            << "  --list-log-levels              List the available s2p log levels\n"
            << "                                 and the current log level.\n"
            << "  --list-operations/-o           List available remote interface operations.\n"
            << "  --list-properties/-P           List the current s2p properties.\n"
            << "  --list-reserved-ids/-I         List reserved device IDs.\n"
            << "  --list-statistics/-S           List s2p statistics.\n"
            << "  --list-settings/-s             List s2p settings.\n"
            << "  --locale LOCALE                Default locale (language)\n"
            << "                                 for client-facing messages.\n"
            << "  --log-level/-L LOG_LEVEL       Log level (trace|debug|info|warning|error|\n"
            << "                                 critical|off), default is 'info'.\n"
            << "  --name/-n VENDOR:PRODUCT:REV   Optional device name for SCSI INQUIRY command\n"
            << "                                 (VENDOR:PRODUCT:REVISION).\n"
            << "  --persist                      Save the current configuration to\n"
            << "                                 /etc/s2p.conf.\n"
            << "  --port/-p PORT                 s2p port to connect to, default is 6868.\n"
            << "  --prompt                       Prompt for the access token in case\n"
            << "                                 s2p requires authentication.\n"
            << "  --rename/-R CURRENT:NEW        Rename an image file.\n"
            << "  --reserved-ids/-r IDS          Comma-separated list of IDs to reserve.\n"
            << "  --scsi-level SCSI_LEVEL        The optional SCSI level, default is SCSI-2.\n"
            << "  --server-version/-V            Display the s2p server version.\n"
            << "  --shut-down/-X                 Shut down s2p.\n"
            << "  --text-protobuf FILENAME       Do not send command to s2p\n"
            << "                                 but write it to a protobuf text file.\n"
            << "  --type/-t DEVICE_TYPE          Optional device type\n"
            << "                                 (sahd|sccd|scdp|schd|schs|sclp|scmo|scrm|scsg|sctp).\n"
            << "  --version/-v                   Display the s2pctl version.\n";
    }
}

int S2pCtl::Run(const vector<char*> &args)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    return args.size() < 2 ? RunInteractive() : ParseArguments(args);
}

int S2pCtl::RunInteractive()
{
    const string &prompt = "s2pctl";

    if (isatty(STDIN_FILENO)) {
        Banner(false);

        cout << "Entering interactive mode, Ctrl-D, \"exit\" or \"quit\" to quit\n";
    }

    while (true) {
        const string &line = GetLine(prompt);
        if (line.empty()) {
            break;
        }

        const auto &args = Split(line, ' ');

        vector<char*> interactive_args;
        interactive_args.emplace_back(strdup(prompt.c_str()));
        interactive_args.emplace_back(strdup(args[0].c_str())
        );
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty()) {
                interactive_args.emplace_back(strdup(args[i].c_str()));
            }
        }

        ParseArguments(interactive_args);
    }

    return EXIT_SUCCESS;
}

int S2pCtl::ParseArguments(const vector<char*> &args) // NOSONAR Acceptable complexity for parsing
{
    const int OPT_PROMPT = 2;
    const int OPT_BINARY_PROTOBUF = 3;
    const int OPT_JSON_PROTOBUF = 4;
    const int OPT_TEXT_PROTOBUF = 5;
    const int OPT_LIST_LOG_LEVELS = 6;
    const int OPT_LOCALE = 7;
    const int OPT_SCSI_LEVEL = 8;
    const int OPT_LIST_EXTENSIONS = 9;
    const int OPT_PERSIST = 10;

    const vector<option> options = {
        { "block-size", required_argument, nullptr, 'b' },
        { "binary-protobuf", required_argument, nullptr, OPT_BINARY_PROTOBUF },
        { "caching-mode", required_argument, nullptr, 'm' },
        { "command", required_argument, nullptr, 'c' },
        { "copy", required_argument, nullptr, 'x' },
        { "create", required_argument, nullptr, 'C' },
        { "delete", required_argument, nullptr, 'd' },
        { "detach-all", no_argument, nullptr, 'D' },
        { "file", required_argument, nullptr, 'f' },
        { "help", no_argument, nullptr, 'h' },
        { "host", required_argument, nullptr, 'H' },
        { "id", required_argument, nullptr, 'i' },
        { "image-folder", required_argument, nullptr, 'F' },
        { "json-protobuf", required_argument, nullptr, OPT_JSON_PROTOBUF },
        { "list-devices", no_argument, nullptr, 'l' },
        { "list-device-types", no_argument, nullptr, 'T' },
        { "list-extensions", no_argument, nullptr, OPT_LIST_EXTENSIONS },
        { "list-images", no_argument, nullptr, 'e' },
        { "list-image-info", required_argument, nullptr, 'E' },
        { "list-interfaces", required_argument, nullptr, 'N' },
        { "list-log-levels", no_argument, nullptr, OPT_LIST_LOG_LEVELS },
        { "list-operations", no_argument, nullptr, 'o' },
        { "list-properties", no_argument, nullptr, 'P' },
        { "list-reserved-ids", no_argument, nullptr, 'I' },
        { "list-settings", no_argument, nullptr, 's' },
        { "list-statistics", no_argument, nullptr, 'S' },
        { "locale", required_argument, nullptr, OPT_LOCALE },
        { "log-level", required_argument, nullptr, 'L' },
        { "name", required_argument, nullptr, 'n' },
        { "persist", no_argument, nullptr, OPT_PERSIST },
        { "port", required_argument, nullptr, 'p' },
        { "prompt", no_argument, nullptr, OPT_PROMPT },
        { "rename", required_argument, nullptr, 'R' },
        { "reserved-ids", optional_argument, nullptr, 'r' },
        { "scsi-level", required_argument, nullptr, OPT_SCSI_LEVEL },
        { "server-version", no_argument, nullptr, 'V' },
        { "shut-down", no_argument, nullptr, 'X' },
        { "text-protobuf", required_argument, nullptr, OPT_TEXT_PROTOBUF },
        { "type", required_argument, nullptr, 't' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    PbCommand command;
    PbDeviceDefinition *device = command.add_devices();
    device->set_id(-1);
    string id_and_lun;
    string params;
    string log_level;
    string default_folder;
    string reserved_ids;
    string image_params;
    string filename;
    string filename_json;
    string filename_binary;
    string filename_text;
    string token;

    string locale = GetLocale();

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(),
        "e::hlos::vDINOPSTVXa:b:-c:d:f:i:m:n:p:r:t:x:C:E:F:H:L:P::R:", options.data(), nullptr)) != -1) {
        switch (opt) { // NOSONAR Acceptable complexity for parsing
        case 'i':
            id_and_lun = optarg;
            break;

        case 'C':
            command.set_operation(CREATE_IMAGE);
            image_params = optarg;
            break;

        case 'b':
            if (const int block_size = ParseAsUnsignedInt(optarg); block_size <= 0) {
                cerr << "Error: Invalid block size " << optarg << '\n';
                return EXIT_FAILURE;
            }
            else {
                device->set_block_size(block_size);
            }
            break;

        case 'c':
            command.set_operation(ParseOperation(optarg));
            if (command.operation() == NO_OPERATION) {
                cerr << "Error: Unknown operation '" << optarg << "'\n";
                return EXIT_FAILURE;
            }
            break;

        case 'D':
            command.set_operation(DETACH_ALL);
            break;

        case 'd':
            command.set_operation(DELETE_IMAGE);
            image_params = optarg;
            break;

        case 'E':
            filename = optarg;
            if (filename.empty()) {
                cerr << "Error: Missing filename\n";
                return EXIT_FAILURE;
            }
            command.set_operation(IMAGE_FILE_INFO);
            break;

        case 'e':
            command.set_operation(DEFAULT_IMAGE_FILES_INFO);
            if (optarg) {
                SetCommandParams(command, optarg);
            }
            break;

        case 'F':
            command.set_operation(DEFAULT_FOLDER);
            default_folder = optarg;
            break;

        case 'f':
            params = optarg;
            break;

        case 'h':
            Banner(true);
            return EXIT_SUCCESS;
            break;

        case 'H':
            hostname = optarg;
            if (hostname.empty()) {
                cerr << "Error: Missing hostname\n";
                return EXIT_FAILURE;
            }
            break;

        case OPT_BINARY_PROTOBUF:
            filename_binary = optarg;
            if (filename_binary.empty()) {
                cerr << "Error: Missing filename\n";
                return EXIT_FAILURE;
            }
            break;

        case OPT_JSON_PROTOBUF:
            filename_json = optarg;
            if (filename_json.empty()) {
                cerr << "Error: Missing filename\n";
                return EXIT_FAILURE;
            }
            break;

        case OPT_TEXT_PROTOBUF:
            filename_text = optarg;
            if (filename_text.empty()) {
                cerr << "Error: Missing filename\n";
                return EXIT_FAILURE;
            }
            break;

        case 'I':
            command.set_operation(RESERVED_IDS_INFO);
            break;

        case 'L':
            command.set_operation(LOG_LEVEL);
            log_level = optarg;
            break;

        case 'l':
            command.set_operation(DEVICES_INFO);
            break;

        case OPT_LIST_EXTENSIONS:
            command.set_operation(MAPPING_INFO);
            break;

        case 'N':
            command.set_operation(NETWORK_INTERFACES_INFO);
            break;

        case OPT_LIST_LOG_LEVELS:
            command.set_operation(LOG_LEVEL_INFO);
            break;

        case 'o':
            command.set_operation(OPERATION_INFO);
            break;

        case 'P':
            command.set_operation(PROPERTIES_INFO);
            break;

        case OPT_PERSIST:
            command.set_operation(PERSIST_CONFIGURATION);
            break;

        case 't':
            device->set_type(ParseDeviceType(optarg));
            if (device->type() == UNDEFINED) {
                cerr << "Error: Invalid device type '" << optarg << "'\n";
                return EXIT_FAILURE;
            }
            break;

        case 'r':
            command.set_operation(RESERVE_IDS);
            reserved_ids = string(optarg) != "\"\"" ? optarg : "";
            break;

        case 'R':
            command.set_operation(RENAME_IMAGE);
            image_params = optarg;
            break;

        case 'm':
            try {
                device->set_caching_mode(ParseCachingMode(optarg));
            }
            catch (const ParserException &e) {
                cerr << "Error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            break;

        case 'n':
            SetProductData(*device, optarg);
            break;

        case 'p':
            port = ParseAsUnsignedInt(optarg);
            if (port <= 0 || port > 65535) {
                cerr << "Error: Invalid port '" << optarg << "', port must be between 1 and 65535\n";
                return EXIT_FAILURE;
            }
            break;

        case 's':
            command.set_operation(SERVER_INFO);
            if (const string &error = SetCommandParams(command, optarg ? optarg : ""); !error.empty()) {
                cerr << "Error: " << error << '\n';
                return EXIT_FAILURE;
            }
            break;

        case 'S':
            command.set_operation(STATISTICS_INFO);
            break;

        case OPT_PROMPT:
            token = optarg ? optarg : getpass("Password: ");
            break;

        case 'x':
            command.set_operation(COPY_IMAGE);
            image_params = optarg;
            break;

        case 'T':
            command.set_operation(DEVICE_TYPES_INFO);
            break;

        case 'v':
            cout << GetVersionString() << '\n';
            return EXIT_SUCCESS;
            break;

        case 'V':
            command.set_operation(VERSION_INFO);
            break;

        case 'X':
            command.set_operation(SHUT_DOWN);
            SetParam(command, "mode", "rascsi");
            break;

        case OPT_SCSI_LEVEL:
            if (const int level = ParseAsUnsignedInt(optarg); level <= 0
                || level >= static_cast<int>(ScsiLevel::LAST)) {
                cerr << "Error: Invalid SCSI level '" << optarg << "'\n";
                return EXIT_FAILURE;
            }
            else {
                device->set_scsi_level(level);
            }
            break;

        case OPT_LOCALE:
            locale = optarg;
            break;

        default:
            Banner(true);
            return EXIT_FAILURE;
        }
    }

    // When no parameters have been provided with the -f option use the free parameter (if present) instead
    if (params.empty() && optind < static_cast<int>(args.size())) {
        params = args.data()[optind];
    }

    if (!id_and_lun.empty()) {
        if (const string &error = SetIdAndLun(*device, id_and_lun); !error.empty()) {
            cerr << "Error: " << error << '\n';
            return EXIT_FAILURE;
        }
    }

    SetParam(command, "token", token);
    SetParam(command, "locale", locale);

    S2pCtlCommands s2pctl_commands(command, hostname, port, filename_binary, filename_json, filename_text);

    bool status = false;
    try {
        // Listing devices is a special case
        if (command.operation() == DEVICES_INFO) {
            command.clear_devices();

            status = s2pctl_commands.HandleDevicesInfo();
        }
        else {
            ParseParameters(*device, params);

            status = s2pctl_commands.Execute(log_level, default_folder, reserved_ids, image_params, filename);
        }
    }
    catch (const IoException &e) {
        cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}

PbOperation S2pCtl::ParseOperation(string_view operation)
{
    const auto &it = OPERATIONS.find(tolower(operation[0]));
    return it != OPERATIONS.end() ? it->second : NO_OPERATION;
}

