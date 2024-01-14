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
#include "s2p/s2p_parser.h"

using namespace std;

void S2pParser::Banner(span<char*> args, bool usage) const
{
    if (usage) {
        cout << "\nUsage: " << args[0] << " [-i|-h ID[:LUN]] FILE] ...\n\n"
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
        cout << s2p_util::Banner("(Target Emulation)") << flush;
    }
}

property_map S2pParser::ParseArguments(span<char*> initial_args, bool &is_sasi)
{
    vector<char*> args = ConvertLegacyOptions(initial_args);

    string id_and_lun;
    string type;
    string product_data;
    string block_size;
    bool has_scsi = false;

    property_map properties;
    properties[PropertyHandler::PROPERTY_FILE] = "";

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-h:-i:b:c:n:p:r:t:z:C:F:L:P:R:v")) != -1) {
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
            is_sasi = true;
            id_and_lun = optarg;
            continue;

        case 'i':
            has_scsi = true;
            id_and_lun = optarg;
            continue;

        case 'n':
            product_data = optarg;
            continue;

        case 't':
            type = optarg;
            continue;

        case 1:
            // Encountered filename
            break;

        default:
            Banner(args, true);
            break;
        }

        if (optopt) {
            Banner(args, false);
            break;
        }

        const string params = optarg;

        if (is_sasi) {
            if (!type.empty() && type != "sahd") {
                has_scsi = true;
            }

            type = "sahd";
        }

        const string device_key = fmt::format("device.{}.", id_and_lun);
        if (!block_size.empty()) {
            properties[device_key + "block_size"] = block_size;
        }
        if (!type.empty()) {
            properties[device_key + "type"] = type;
        }
        if (!params.empty()) {
            properties[device_key + "params"] = params;
        }
        if (!product_data.empty()) {
            properties[device_key + "product_data"] = product_data;
        }

        id_and_lun = "";
        type = "";
        product_data = "";
        block_size = "";
    }

    if (has_scsi && is_sasi) {
        throw parser_exception("SCSI and SASI devices cannot be mixed");
    }

    return properties;
}

vector<char*> S2pParser::ConvertLegacyOptions(const span<char*> &initial_args)
{
    // Convert some legacy RaSCSI/PiSCSI options to a consistent getopt() format:
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
