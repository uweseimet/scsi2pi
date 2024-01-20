//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2020-2023 Contributors to the PiSCSI project
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <clocale>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <getopt.h>
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "shared/s2p_version.h"
#include "shared_protobuf/protobuf_util.h"
#include "s2pctl_parser.h"
#include "s2pctl_commands.h"
#include "s2pctl_core.h"

using namespace std;
using namespace s2p_interface;
using namespace s2p_util;
using namespace protobuf_util;

void S2pCtl::Banner(bool usage) const
{
    cout << s2p_util::Banner("(Server Controller Tool)", false);

    if (usage) {
        cout << "Usage: s2pctl [options]\n"
            << "  --id/-i ID[:LUN]               Target device ID (0-7) and LUN\n"
            << "                                 (SCSI: 0-31, SASI: 0-1), default LUN is 0.\n"
            << "  --command/-c COMMAND           Command (attach|detach|insert|eject|protect\n"
            << "                                 |unprotect).\n"
            << "  --type/-t TYPE                 Optional device type\n"
            << "                                 (schd|scrm|sccd|scmo|scdp|sclp|schs|sahd).\n"
            << "  --block-size/-b BLOCK_SIZE     Optional block size\n"
            << "                                 (256|512|1024|2048|4096).\n"
            << "  --name/-n PRODUCT_DATA         Optional product data for SCSI INQUIRY command\n"
            << "                                 (VENDOR:PRODUCT:REVISION).\n"
            << "  --file/-f FILE|PARAM           Image file path or device-specific parameter.\n"
            << "  --image-folder/-F FOLDER       Default location for image files,\n"
            << "                                 default is '~/images'.\n"
            << "  --log-level/-L LOG_LEVEL       Log level (trace|debug|info|warning|\n"
            << "                                 error|off), default is 'info'.\n"
            << "  --help/-h                      Display usage information.\n"
            << "  --host/-H HOST                 s2p host to connect to, default is 'localhost'.\n"
            << "  --port/-p PORT                 s2p port to connect to, default is 6868.\n"
            << "  --reserve-ids/-r IDS           Comma-separated list of IDs to reserve.\n"
            << "  --create/-C FILENAME:SIZE      Create an empty image file.\n"
            << "  --delete/-d FILENAME           Delete an image file.\n"
            << "  --binary-protobuf FILENAME     Do not send command to s2p\n"
            << "                                 but write it to a protobuf binary file.\n"
            << "  --json-protobuf FILENAME       Do not send command to s2p\n"
            << "                                 but write it to a protobuf JSON file.\n"
            << "  --text-protobuf FILENAME       Do not send command to s2p\n"
            << "                                 but write it to a protobuf text file.\n"
            << "  --rename/-R CURRENT:NEW        Rename an image file.\n"
            << "  --copy/-x CURRENT:NEW          Copy an image file.\n"
            << "  --locale LOCALE                Default locale (language)\n"
            << "                                 for client-facing messages.\n"
            << "  --list-images/-e               List images files in the default image folder.\n"
            << "  --list-image-info/-E FILENAME  Display image file information.\n"
            << "  --detach-all/-D                Detach all devices.\n"
            << "  --list-reserved-ids/-I         List reserved device IDs.\n"
            << "  --list-devices/-l              Display device list.\n"
            << "  --list-device-types/-T         List available device types.\n"
            << "  --list-extensions/-m           List supported file extensions\n"
            << "                                 and the device types they map to.\n"
            << "  --list-interfaces/-N           List network interfaces that are up.\n"
            << "  --list-operations/-o           List available remote interface operations.\n"
            << "  --list-properties/-P           List s2p startup properties.\n"
            << "  --list-log-levels              List the available s2p log levels\n"
            << "                                 and the current log level.\n"
            << "  --prompt                       Prompt for the access token in case\n"
            << "                                 s2p requires authentication.\n"
            << "  --list-settings/-s             List s2p settings.\n"
            << "  --list-statistics/-S           List s2p statistics.\n"
            << "  --version/-v                   Display the s2pctl version.\n"
            << "  --server-version/-V            Display the s2p server version.\n"
            << "  --shut-down/-X                 Shut down s2p.\n";
    }
}

int S2pCtl::Run(const vector<char*> &args) const
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    return args.size() < 2 ? RunInteractive() : ParseArguments(args, false);
}

int S2pCtl::RunInteractive() const
{
    if (isatty(STDIN_FILENO)) {
        Banner(false);

        cout << "Entering interactive mode, Ctrl-D or \"exit\" to quit\n";
    }

    while (true) {
        if (isatty(STDIN_FILENO)) {
            cout << "s2pctl>";
        }

        string line;
        if (!getline(cin, line) || line == "exit") {
            if (line.empty() && isatty(STDIN_FILENO)) {
                cout << "\n";
            }
            break;
        }

        if (!line.starts_with("#")) {
            const auto &args = Split(line, ' ');

            vector<char*> interactive_args;
            interactive_args.emplace_back(strdup("s2pctl"));

            // The command must have exactly one leading hyphen because it will be parsed with getopt_long_only()
            string command = args[0];
            command = "-" + command.erase(0, command.find_first_not_of('-'));
            interactive_args.emplace_back(strdup(command.c_str()));

            for (size_t i = 1; i < args.size(); i++) {
                interactive_args.emplace_back(strdup(args[i].c_str()));
            }

            ParseArguments(interactive_args, true);
        }
    }

    return EXIT_SUCCESS;
}

int S2pCtl::ParseArguments(const vector<char*> &args, bool interactive) const
{
    const int OPT_PROMPT = 2;
    const int OPT_BINARY_PROTOBUF = 3;
    const int OPT_JSON_PROTOBUF = 4;
    const int OPT_TEXT_PROTOBUF = 5;
    const int OPT_LIST_LOG_LEVELS = 6;
    const int OPT_LOCALE = 7;

    const vector<option> options = {
        { "prompt", no_argument, nullptr, OPT_PROMPT },
        { "binary-protobuf", required_argument, nullptr, OPT_BINARY_PROTOBUF },
        { "block-size", required_argument, nullptr, 'b' },
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
        { "list-extensions", no_argument, nullptr, 'm' },
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
        { "port", required_argument, nullptr, 'p' },
        { "rename", required_argument, nullptr, 'R' },
        { "reserve-ids", optional_argument, nullptr, 'r' },
        { "server-version", no_argument, nullptr, 'V' },
        { "shut-down", no_argument, nullptr, 'X' },
        { "text-protobuf", required_argument, nullptr, OPT_TEXT_PROTOBUF },
        { "type", required_argument, nullptr, 't' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    S2pCtlParser parser;
    PbCommand command;
    PbDeviceDefinition *device = command.add_devices();
    device->set_id(-1);
    string hostname = "localhost";
    int port = 6868;
    string id_and_lun;
    string param;
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

    const auto &get_opt = interactive ? getopt_long_only : getopt_long;

    optind = 1;
    int opt;
    while ((opt = get_opt(static_cast<int>(args.size()), args.data(),
        "e::hlmos::vDINOPSTVXa:b:c:d:f:i:n:p:r:t:x:C:E:F:H:L:P::R:", options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'i':
            id_and_lun = optarg;
            break;

        case 'C':
            command.set_operation(CREATE_IMAGE);
            image_params = optarg;
            break;

        case 'b':
            int block_size;
            if (!GetAsUnsignedInt(optarg, block_size)) {
                cerr << "Error: Invalid block size " << optarg << endl;
                return EXIT_FAILURE;
            }
            device->set_block_size(block_size);
            break;

        case 'c':
            command.set_operation(parser.ParseOperation(optarg));
            if (command.operation() == NO_OPERATION) {
                cerr << "Error: Unknown operation '" << optarg << "'" << endl;
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
                cerr << "Error: Missing filename" << endl;
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
            param = optarg;
            break;

        case 'h':
            Banner(true);
            return EXIT_SUCCESS;
            break;

        case 'H':
            hostname = optarg;
            if (hostname.empty()) {
                cerr << "Error: Missing hostname" << endl;
                return EXIT_FAILURE;
            }
            break;

        case OPT_BINARY_PROTOBUF:
            filename_binary = optarg;
            if (filename_binary.empty()) {
                cerr << "Error: Missing filename" << endl;
                return EXIT_FAILURE;
            }
            break;

        case OPT_JSON_PROTOBUF:
            filename_json = optarg;
            if (filename_json.empty()) {
                cerr << "Error: Missing filename" << endl;
                return EXIT_FAILURE;
            }
            break;

        case OPT_TEXT_PROTOBUF:
            filename_text = optarg;
            if (filename_text.empty()) {
                cerr << "Error: Missing filename" << endl;
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

        case 'm':
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

        case 't':
            device->set_type(parser.ParseType(optarg));
            if (device->type() == UNDEFINED) {
                cerr << "Error: Unknown device type '" << optarg << "'" << endl;
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

        case 'n':
            SetProductData(*device, optarg);
            break;

        case 'p':
            if (!GetAsUnsignedInt(optarg, port) || port <= 0 || port > 65535) {
                cerr << "Error: Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
                return EXIT_FAILURE;
            }
            break;

        case 's':
            command.set_operation(SERVER_INFO);
            if (optarg) {
                if (const string error = SetCommandParams(command, optarg); !error.empty()) {
                    cerr << "Error: " << error << endl;
                    return EXIT_FAILURE;
                }
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

        case OPT_LOCALE:
            locale = optarg;
            break;

        default:
            Banner(true);
            return EXIT_FAILURE;
        }
    }

    if (!id_and_lun.empty()) {
        if (const string error = SetIdAndLun(8, device->type() == PbDeviceType::SAHD ? 2 : 32, *device, id_and_lun); !error.empty()) {
            cerr << "Error: " << error << endl;
            return EXIT_FAILURE;
        }
    }

    SetParam(command, "token", token);
    SetParam(command, "locale", locale);

    S2pCtlCommands s2pctl_commands(command, hostname, port, filename_binary, filename_json, filename_text);

    bool status;
    try {
        // Listing devices is a special case
        if (command.operation() == DEVICES_INFO) {
            command.clear_devices();

            status = s2pctl_commands.CommandDevicesInfo();
        }
        else {
            ParseParameters(*device, param);

            status = s2pctl_commands.Execute(log_level, default_folder, reserved_ids, image_params, filename);
        }
    }
    catch (const io_exception &e) {
        cerr << "Error: " << e.what() << endl;

        status = false;

        // Fall through
    }

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
