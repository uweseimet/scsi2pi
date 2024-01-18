//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include "shared/shared_exceptions.h"
#include "controllers/controller_factory.h"
#include "shared/s2p_util.h"
#include "s2p/s2p_parser.h"

using namespace std;
using namespace s2p_util;

void S2pParser::Banner(bool usage) const
{
    if (usage) {
        cout << "\nUsage: s2p [-i|-h ID[:LUN]] FILE] ...\n\n"
            << " -h ID is a SCSI device ID (0-" << (ControllerFactory::GetIdMax() - 1) << ").\n"
            << " -i ID is a SASI device ID (0-" << (ControllerFactory::GetIdMax() - 1) << ").\n"
            << " LUN is the optional logical unit, 0 is the default"
            << " (SCSI: 0-" << (ControllerFactory::GetScsiLunMax() - 1)
            << ", SASI: 0-" << (ControllerFactory::GetSasiLunMax() - 1) << ").\n"
            << " Attaching a SASI drive (-h instead of -i) selects SASI compatibility.\n"
            << " FILE is either a disk image file, \"daynaport\", \"printer\" or \"services\".\n"
            << " The image type is derived from the extension when no type is specified:\n"
            << "  hd1: SCSI HD image (Non-removable SCSI-1-CCS HD image)\n"
            << "  hds: SCSI HD image (Non-removable SCSI-2 HD image)\n"
            << "  hda: SCSI HD image (Apple compatible non-removable SCSI-2 HD image)\n"
            << "  hdr: SCSI HD image (Removable SCSI-2 HD image)\n"
            << "  mos: SCSI MO image (SCSI-2 MO image)\n"
            << "  iso: SCSI CD image (SCSI-2 ISO 9660 image)\n"
            << "  is1: SCSI CD image (SCSI-1-CCS ISO 9660 image)\n"
            << " Run 'man s2p' for other options.\n" << flush;

        exit(EXIT_SUCCESS);
    }
    else {
        cout << s2p_util::Banner("(Device Emulation)", false) << flush;
    }
}

property_map S2pParser::ParseArguments(span<char*> initial_args, bool &has_sasi)
{
    vector<char*> args = ConvertLegacyOptions(initial_args);

    string id_lun;
    string type;
    string product_data;
    string block_size;
    bool blue_scsi_mode = false;
    bool has_scsi = false;

    property_map properties;
    properties[PropertyHandler::PROPERTY_FILES] = "";

    optind = 1;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-h:-i:b:c:n:p:r:t:z:C:F:L:P:R:vB")) != -1) {
        if (const auto &property = OPTIONS_TO_PROPERTIES.find(opt); property != OPTIONS_TO_PROPERTIES.end()) {
            properties[property->second] = optarg;
            continue;
        }

        // The remaining options are device-related
        switch (opt) {
        case 'b':
            block_size = optarg;
            continue;

        case 'h':
            id_lun = optarg;
            has_sasi = true;
            type = "sahd";
            continue;

        case 'i':
            id_lun = optarg;
            has_scsi = true;
            continue;

        case 'n':
            product_data = optarg;
            continue;

        case 't':
            ranges::transform(string(optarg), back_inserter(type), ::tolower);
            continue;

        case 'B':
            blue_scsi_mode = true;
            continue;

        case 1:
            // Encountered filename
            break;

        default:
            Banner(true);
            break;
        }

        if (optopt) {
            Banner(false);
            break;
        }

        if ((has_scsi && type == "sahd") || (has_sasi && (type.empty() || type != "sahd"))) {
            throw parser_exception("SCSI and SASI devices cannot be mixed");
        }

        string device_key;
        if (!id_lun.empty()) {
            device_key = fmt::format("device.{}.", id_lun);
        }

        const string &params = optarg;
        if (blue_scsi_mode && !params.empty()) {
            device_key = ParseBlueScsiFilename(properties, device_key, params, has_sasi);
        }

        if (!block_size.empty()) {
            properties[device_key + "block_size"] = block_size;
        }
        if (!type.empty()) {
            properties[device_key + "type"] = type;
        }
        if (!product_data.empty()) {
            properties[device_key + "product_data"] = product_data;
        }
        if (!params.empty()) {
            properties[device_key + "params"] = params;
        }

        id_lun = "";
        type = "";
        product_data = "";
        block_size = "";
    }

    return properties;
}

string S2pParser::ParseBlueScsiFilename(property_map &properties, const string &d, const string &filename, bool is_sasi)
{
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
        string lun = "";
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
    if (t->second.empty()) {
        throw parser_exception(fmt::format("Unsupported BlueSCSI device type: '{}'", type));
    }
    if (t->second != "schd") {
        properties[device_key + "type"] = t->second;
    }
    else {
        properties[device_key + "type"] = is_sasi ? "sahd" : "schd";
    }

    string block_size = "512";
    if (components.size() > 1) {
        if (const string b = ParseNumber(components[1]); !b.empty()) {
            block_size = b;
        }
        // When there is no block_size number after the "_" separator the string is the product data
        else {
            properties[device_key + "product_data"] = components[1];
        }
    }
    properties[device_key + "block_size"] = block_size;

    if (components.size() > 2) {
        properties[device_key + "product_data"] = components[2];
    }

    return device_key;
}

vector<char*> S2pParser::ConvertLegacyOptions(const span<char*> &initial_args)
{
    // Convert some legacy RaSCSI/PiSCSI options to a consistent getopt() format:
    //   -id|-ID -> -i
    //   -hd|-HD -> -h
    //   -idn:u|-hdn:u -> -i|-h n:u
    vector<char*> args;
    for (const string &arg : initial_args) {
        int start_of_ids = -1;
        for (int i = 0; i < static_cast<int>(arg.length()); i++) {
            if (isdigit(arg[i])) {
                start_of_ids = i;
                break;
            }
        }

        const string ids = start_of_ids != -1 ? arg.substr(start_of_ids) : "";

        string arg_lower;
        ranges::transform(arg, back_inserter(arg_lower), ::tolower);

        if (arg_lower.starts_with("-h")) {
            args.emplace_back(strdup("-h"));
            if (!ids.empty()) {
                args.emplace_back(strdup(ids.c_str()));
            }
        }
        else if (arg_lower.starts_with("-i")) {
            args.emplace_back(strdup("-i"));
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
    size_t i = 0;
    while (s.size() > i) {
        if (!isdigit(s[i])) {
            break;
        }

        result += s[i++];
    }

    return result;
}
