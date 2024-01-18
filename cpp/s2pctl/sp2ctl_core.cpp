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

#include <unistd.h>
#include <clocale>
#include <iostream>
#include <fstream>
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

void ScsiCtl::Banner(bool usage) const
{
    cout << s2p_util::Banner("(Server Controller Tool)", false);

    if (usage) {
        cout << "Usage: s2pctl [options]\n"
            << "  -i ID[:LUN]               Target device ID (0-7) and LUN\n"
            << "                            (SCSI: 0-31, SASI: 0-1).\n"
            << "  -c CMD                    Command (attach|detach|insert|eject|protect\n"
            << "                            |unprotect|show).\n"
            << "  -t TYPE                   Optional device type\n"
            << "                            (schd|scrm|sccd|scmo|scdp|sclp|schs|sahd).\n"
            << "  -b BLOCK_SIZE             Optional block size\n"
            << "                            (256|512|1024|2048|4096).\n"
            << "  -n NAME]                  Product data (VENDOR:PRODUCT:REVISION).\n"
            << "  -f FILE|PARAM             Image file path or device-specific parameter.\n"
            << "  -F IMAGE_FOLDER           Default location for image files,\n"
            << "                            default is '~/images'.\n"
            << "  -L LOG_LEVEL              Log level (trace|debug|info|warning|\n"
            << "                            error|off), default is 'info'.\n"
            << "  -h                        Display usage information.\n"
            << "  -H HOST                   s2p host to connect to, default is 'localhost'.\n"
            << "  -p PORT                   s2p port to connect to, default is 6868.\n"
            << "  -r RESERVED_IDS           Comma-separated list of IDs to reserve.\n"
            << "  -C FILENAME:FILESIZE      Create an empty image file.\n"
            << "  -d FILENAME               Delete an image file.\n"
            << "  -B FILENAME               Do not send command to s2p\n"
            << "                            but write it to a protobuf binary file.\n"
            << "  -J FILENAME               Do not send command to s2p\n"
            << "                            but write it to a protobuf JSON file.\n"
            << "  -T FILENAME               Do not send command to s2p\n"
            << "                            but write it to a protobuf text file.\n"
            << "  -R CURRENT_NAME:NEW_NAME  Rename an image file.\n"
            << "  -x CURRENT_NAME:NEW_NAME  Copy an image file.\n"
            << "  -z LOCALE                 Select response locale/language.\n"
            << "  -e                        List all images files in the default image folder.\n"
            << "  -E FILENAME               Display image file information.\n"
            << "  -D                        Detach all devices.\n"
            << "  -I                        Display reserved device IDs.\n"
            << "  -l                        Display device list.\n"
            << "  -m                        List all supported file extensions\n"
            << "                            and the device types they map to.\n"
            << "  -o                        Display operation meta data.\n"
            << "  -q                        Display s2p startup properties.\n"
            << "  -O                        Display the available s2p log levels\n"
            << "                            and the current log level.\n"
            << "  -P                        Prompt for the access token in case\n"
            << "                            s2p requires authentication.\n"
            << "  -s                        Display all s2p settings.\n"
            << "  -S                        Display s2p statistics.\n"
            << "  -V                        Display the s2p server version.\n"
            << "  -v                        Display the s2pctl version.\n"
            << "  -X                        Shut down s2p.\n"
            << " If CMD is 'attach' or 'insert' the FILE parameter is required.\n";
    }
}

int ScsiCtl::Run(const vector<char*> &args) const
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    return args.size() < 2 ? RunInteractive() : RunNonInteractive(args);
}

int ScsiCtl::RunInteractive() const
{
    Banner(false);

    cout << "Entering interactive mode, quit with Ctrl-D\n";

    while (true) {
        cout << "s2pctl>";

        string line;
        if (!getline(cin, line)) {
            break;
        }

        vector<char*> args;
        args.emplace_back(strdup("arg0"));
        for (const string &arg : Split(line, ' ')) {
            args.emplace_back(strdup(arg.c_str()));
        }

        RunNonInteractive(args);
    }

    cout << "\n";

    return EXIT_SUCCESS;
}

int ScsiCtl::RunNonInteractive(const vector<char*> &args) const
{
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
    bool list = false;

    string locale = GetLocale();

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(),
        "e::hlmoqs::vDINOSTVXa:b:c:d:f:i:n:p:r:t:x:z:B:C:E:F:H:J:L:P::R:Z:")) != -1) {
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

        case 'B':
            filename_binary = optarg;
            if (filename_binary.empty()) {
                cerr << "Error: Missing filename" << endl;
                return EXIT_FAILURE;
            }
            break;

        case 'J':
            filename_json = optarg;
            if (filename_json.empty()) {
                cerr << "Error: Missing filename" << endl;
                return EXIT_FAILURE;
            }
            break;

        case 'Z':
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
            list = true;
            break;

        case 'm':
            command.set_operation(MAPPING_INFO);
            break;

        case 'N':
            command.set_operation(NETWORK_INTERFACES_INFO);
            break;

        case 'O':
            command.set_operation(LOG_LEVEL_INFO);
            break;

        case 'o':
            command.set_operation(OPERATION_INFO);
            break;

        case 'q':
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
            reserved_ids = optarg;
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

        case 'P':
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
            cout << "s2pctl version: " << GetVersionString() << '\n';
            return EXIT_SUCCESS;
            break;

        case 'V':
            command.set_operation(VERSION_INFO);
            break;

        case 'X':
            command.set_operation(SHUT_DOWN);
            SetParam(command, "mode", "rascsi");
            break;

        case 'z':
            locale = optarg;
            break;

        default:
            break;
        }
    }

    // BSD getopt stops after the first free argument. Thie work-around below cannot really address this.
#ifdef __linux__
    if (optopt) {
        exit(EXIT_FAILURE);
    }
#endif

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
        // Listing devices is a special case (legacy rasctl backwards compatibility)
        if (list) {
            command.clear_devices();
            command.set_operation(DEVICES_INFO);

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
