//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <spdlog/spdlog.h>
#include "base/property_handler.h"
#include "controllers/controller_factory.h"
#include "shared/s2p_exceptions.h"
#include "s2p/s2p_parser.h"

using namespace s2p_util;

void S2pParser::Banner(bool usage) const
{
    if (!usage) {
        cout << s2p_util::Banner("(Device Emulation)") << flush;
    }
    else {
        cout << "Usage: s2p options ... FILE\n"
            << "  --scsi-id/-i ID[:LUN]       SCSI target device ID (0-7) and LUN (0-7),\n"
            << "                              default LUN is 0.\n"
            << "  --sasi-id/-h ID[:LUN]       SASI target device ID (0-7) and LUN (0-1),\n"
            << "                              default LUN is 0.\n"
            << "  --type/-t TYPE              Device type.\n"
            << "  --scsi-level LEVEL          Optional SCSI standard level (1-8),\n"
            << "                              default is device-specific and usually SCSI-2.\n"
            << "  --name/-n PRODUCT_NAME      Optional product name for SCSI INQUIRY command,\n"
            << "                              format is VENDOR:PRODUCT:REVISION.\n"
            << "  --block-size/-b BLOCK_SIZE  Optional block (sector) size.\n"
            << "  --caching-mode/-m MODE      Caching mode (piscsi|write-through|linux\n"
            << "                              |linux-optimized), default currently is PiSCSI\n"
            << "                              compatible caching.\n"
            << "  --blue-scsi-mode/-B         Enable BlueSCSI filename compatibility mode.\n"
            << "  --reserved-ids/-r IDS       List of IDs to reserve.\n"
            << "  --image-folder/-F FOLDER    Default folder with image files.\n"
            << "  --scan-depth/-R DEPTH       Scan depth for image file folder.\n"
            << "  --property/-c KEY=VALUE     Sets a configuration property.\n"
            << "  --property-files/-C         List of configuration property files.\n"
            << "  --log-level/-L LEVEL        Log level (trace|debug|info|warning|error|\n"
            << "                              critical|off),\n"
            << "                              default is 'info'.\n"
            << "  --log-pattern/-l PATTERN    The spdlog pattern to use for logging.\n"
            << "  --token-file/-P FILE        Access token file.\n"
            << "  --port/-p PORT              s2p server port, default is 6868.\n"
            << "  --ignore-conf               Ignore /etc/s2p.conf and ~/.config/s2p.conf.\n"
            << "  --version/-v                Display the program version.\n"
            << "  --help                      Display this help.\n"
            << "  FILE is either a drive image file, 'daynaport', 'printer' or 'services'.\n"
            << "  If no type is specific the image type is derived from the extension:\n"
            << "    hd1: SCSI HD image (Non-removable SCSI-1-CCS HD image)\n"
            << "    hds: SCSI HD image (Non-removable SCSI-2 HD image)\n"
            << "    hda: SCSI HD image (Apple compatible non-removable SCSI-2 HD image)\n"
            << "    hdr: SCSI HD image (Removable SCSI-2 HD image)\n"
            << "    mos: SCSI MO image (SCSI-2 MO image)\n"
            << "    iso: SCSI CD image (SCSI-2 ISO 9660 image)\n"
            << "    is1: SCSI CD image (SCSI-1-CCS ISO 9660 image)\n";
    }
}

property_map S2pParser::ParseArguments(span<char*> initial_args, bool &ignore_conf) const // NOSONAR Acceptable complexity for parsing
{
    const int OPT_SCSI_LEVEL = 2;
    const int OPT_IGNORE_CONF = 3;
    const int OPT_HELP = 4;

    const vector<option> options = {
        { "block-size", required_argument, nullptr, 'b' },
        { "blue-scsi-mode", no_argument, nullptr, 'B' },
        { "caching-mode", required_argument, nullptr, 'm' },
        { "image-folder", required_argument, nullptr, 'F' },
        { "help", no_argument, nullptr, OPT_HELP },
        { "ignore-conf", no_argument, nullptr, OPT_IGNORE_CONF },
        { "locale", required_argument, nullptr, 'z' },
        { "log-level", required_argument, nullptr, 'L' },
        { "log-pattern", required_argument, nullptr, 'l' },
        { "name", required_argument, nullptr, 'n' },
        { "port", required_argument, nullptr, 'p' },
        { "property", required_argument, nullptr, 'c' },
        { "property-files", required_argument, nullptr, 'C' },
        { "reserved-ids", optional_argument, nullptr, 'r' },
        { "sasi-id", required_argument, nullptr, 'h' },
        { "scan-depth", required_argument, nullptr, 'R' },
        { "scsi-id", required_argument, nullptr, 'i' },
        { "scsi-level", required_argument, nullptr, OPT_SCSI_LEVEL },
        { "token-file", required_argument, nullptr, 'P' },
        { "type", required_argument, nullptr, 't' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    const unordered_map<int, const char*> OPTIONS_TO_PROPERTIES = {
        { 'p', PropertyHandler::PORT },
        { 'r', PropertyHandler::RESERVED_IDS },
        { 'z', PropertyHandler::LOCALE },
        { 'C', PropertyHandler::PROPERTY_FILES },
        { 'F', PropertyHandler::IMAGE_FOLDER },
        { 'L', PropertyHandler::LOG_LEVEL },
        { 'l', PropertyHandler::LOG_PATTERN },
        { 'P', PropertyHandler::TOKEN_FILE },
        { 'R', PropertyHandler::SCAN_DEPTH }
    };

    vector<char*> args = ConvertLegacyOptions(initial_args);

    string id_lun;
    string type;
    string scsi_level;
    string name;
    string block_size;
    string caching_mode;
    bool blue_scsi_mode = false;

    property_map properties;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "-h:-i:b:c:l:m:n:p:r:t:z:C:F:L:P:R:B",
        options.data(), nullptr)) != -1) {
        if (const auto &property = OPTIONS_TO_PROPERTIES.find(opt); property != OPTIONS_TO_PROPERTIES.end()) {
            properties[property->second] = optarg;
            continue;
        }

        // The remaining options are device-related
        switch (opt) {
        case 'b':
            block_size = optarg;
            continue;

        case 'B':
            blue_scsi_mode = true;
            continue;

        case 'c':
            if (const auto &key_value = Split(optarg, '=', 2); key_value.size() < 2 || key_value[0].empty()) {
                throw parser_exception("Invalid property '" + string(optarg) + "'");
            }
            else {
                properties[key_value[0]] = key_value[1];
            }
            continue;

        case 'h':
            id_lun = optarg;
            type = "sahd";
            continue;

        case 'i':
            id_lun = optarg;
            continue;

        case 'm':
            caching_mode = optarg;
            continue;

        case 'n':
            name = optarg;
            continue;

        case 't':
            type = ToLower(optarg);
            continue;

        case OPT_SCSI_LEVEL:
            scsi_level = optarg;
            continue;

        case OPT_IGNORE_CONF:
            ignore_conf = true;
            continue;

        case OPT_HELP:
            Banner(true);
            exit(EXIT_SUCCESS);
            break;

        case 1:
            // Encountered a free parameter e.g. a filename
            break;

        default:
            Banner(true);
            exit(EXIT_FAILURE);
            break;
        }

        string device_key;
        if (!id_lun.empty()) {
            device_key = fmt::format("device.{}.", id_lun);
        }

        const string &params = optarg;
        if (blue_scsi_mode && !params.empty()) {
            device_key = ParseBlueScsiFilename(properties, device_key, params);
        }

        if (!block_size.empty()) {
            properties[device_key + PropertyHandler::BLOCK_SIZE] = block_size;
            block_size.clear();
        }
        if (!caching_mode.empty()) {
            properties[device_key + PropertyHandler::CACHING_MODE] = caching_mode;
            caching_mode.clear();
        }
        if (!type.empty()) {
            properties[device_key + PropertyHandler::TYPE] = type;
            type.clear();
        }
        if (!scsi_level.empty()) {
            properties[device_key + PropertyHandler::SCSI_LEVEL] = scsi_level;
            scsi_level.clear();
        }
        if (!name.empty()) {
            properties[device_key + PropertyHandler::NAME] = name;
            name.clear();
        }
        if (!params.empty()) {
            properties[device_key + PropertyHandler::PARAMS] = params;
        }

        id_lun.clear();
    }

    return properties;
}

string S2pParser::ParseBlueScsiFilename(property_map &properties, const string &d, const string &filename)
{
    const unordered_map<string, const char*, s2p_util::StringHash, equal_to<>> BLUE_SCSI_TO_S2P_TYPES = {
        { "CD", "sccd" },
        { "FD", "schd" },
        { "HD", "schd" },
        { "MO", "scmo" },
        { "RE", "scrm" },
        { "TP", nullptr }
    };

    const auto index = filename.find(".");
    const string &specifier = index == string::npos ? filename : filename.substr(0, index);
    const auto &components = Split(specifier, '_');

    const string &type_id_lun = components[0];
    if (type_id_lun.size() < 3) {
        throw parser_exception(fmt::format("Invalid BlueSCSI filename format: '{}'", specifier));
    }

    // An explicit ID/LUN on the command line overrides the BlueSCSI ID/LUN
    string device_key = d;
    if (d.empty()) {
        const char id = type_id_lun[2];
        string lun;
        if (type_id_lun.size() > 3) {
            lun = ParseNumber(type_id_lun.substr(3));
        }
        lun = !lun.empty() && lun != "0" ? ":" + lun : "";
        device_key = fmt::format("device.{0}{1}.", id, lun);
    }

    const string &type = type_id_lun.substr(0, 2);
    const auto &t = BLUE_SCSI_TO_S2P_TYPES.find(type);
    if (t == BLUE_SCSI_TO_S2P_TYPES.end()) {
        throw parser_exception(fmt::format("Invalid BlueSCSI device type: '{}'", type));
    }
    if (!t->second) {
        throw parser_exception(fmt::format("Unsupported BlueSCSI device type: '{}'", type));
    }
    properties[device_key + PropertyHandler::TYPE] = t->second;

    string block_size = "512";
    if (components.size() > 1) {
        if (const string b = ParseNumber(components[1]); !b.empty()) {
            block_size = b;
        }
        // When there is no block_size number after the "_" separator the string is the product data
        else {
            properties[device_key + PropertyHandler::NAME] = components[1];
        }
    }
    properties[device_key + PropertyHandler::BLOCK_SIZE] = block_size;

    if (components.size() > 2) {
        properties[device_key + PropertyHandler::NAME] = components[2];
    }

    return device_key;
}

vector<char*> S2pParser::ConvertLegacyOptions(const span<char*> &initial_args)
{
    // Convert legacy RaSCSI/PiSCSI ID options to a consistent getopt() format:
    //   -id|-ID -> -i
    //   -hd|-HD -> -h
    //   -idn:u|-hdn:u -> -i|-h n:u
    vector<char*> args;
    for (const string arg : initial_args) {
        int start_of_ids = -1;
        for (int i = 0; i < static_cast<int>(arg.length()); i++) {
            if (isdigit(arg[i])) {
                start_of_ids = i;
                break;
            }
        }

        const string &ids = start_of_ids != -1 ? arg.substr(start_of_ids) : "";

        const string &arg_lower = ToLower(arg);
        if (arg_lower.starts_with("-h") || arg_lower.starts_with("-i")) {
            args.emplace_back(strdup(arg_lower.substr(0, 2).c_str()));
            if (!ids.empty()) {
                args.emplace_back(strdup(ids.c_str()));
            }
        }
        else {
            args.emplace_back(strdup(arg.c_str()));
        }
    }

    return args;
}

string S2pParser::ParseNumber(const string &s)
{
    string result;
    size_t i = -1;
    while (s.size() > ++i) {
        if (!isdigit(s[i])) {
            break;
        }

        result += s[i];
    }

    return result;
}
