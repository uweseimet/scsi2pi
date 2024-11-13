//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"
#include <cassert>
#include <filesystem>
#include <getopt.h>
#include "shared/s2p_util.h"

using namespace s2p_util;
using namespace simh_util;

void S2pSimh::Banner(bool help)
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Simh .tap File Analysis Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2024 Uwe Seimet\n";

    if (help) {
        cout << "Usage: s2psimh <SIMH_TAP_FILE>\n"
            << "  --dump/-d         Dump record contents.\n"
            << "  --limit/-l LIMIT  Limit record contents dump size to LIMIT bytes.\n"
            << "  --version/-v      Display the program version.\n"
            << "  --help/-h         Display this help.\n";
    }
}

bool S2pSimh::ParseArguments(span<char*> args)
{
    const vector<option> options = {
        { "dump", no_argument, nullptr, 'd' },
        { "limit", required_argument, nullptr, 'l' },
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    bool version = false;
    bool help = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "-dhl:v", options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'd':
            dump = true;
            break;

        case 'l':
            if (!GetAsUnsignedInt(string(optarg), limit)) {
                cerr << "Error: Invalid dump size limit " << optarg << endl;
                return false;
            }
            break;

        case 'h':
            help = true;
            break;

        case 'v':
            version = true;
            break;

        case 1:
            filename = optarg;
            break;

        default:
            Banner(false);
            return false;
        }
    }

    if (help) {
        Banner(true);
        return false;
    }

    if (version) {
        cout << GetVersionString() << '\n';
        return false;
    }

    if (filename.empty()) {
        Banner(true);
        return false;
    }

    return true;
}

int S2pSimh::Run(span<char*> args)
{
    if (!ParseArguments(args)) {
        return EXIT_SUCCESS;
    }

    file.open(filename, ios::in | ios::binary);
    if (file.fail()) {
        cerr << "Error: Can't open '" << filename << "'" << endl;
        return EXIT_FAILURE;
    }

    off_t file_size;
    try {
        file_size = filesystem::file_size(filename);
    }
    catch (const filesystem::filesystem_error &e) {
        cerr << "Error: Can't get size of '" << filename << "': " << e.what();
        return EXIT_FAILURE;
    }

    return Analyze(file, file_size);
}

int S2pSimh::Analyze(istream &file, off_t size)
{
    while (offset < size) {
        old_offset = offset;

        const auto [cls, value] = ReadHeader(file, offset);
        switch (cls) {
        case simh_class::tape_mark_good_data_record:
            PrintClass(cls);
            if (!value) {
                cout << ", tape mark\n";
            }
            else {
                cout << ", good data record, record length";
                if (!PrintRecord(value)) {
                    return EXIT_FAILURE;
                }
            }
            break;

        case simh_class::bad_data_record:
            PrintClass(cls);
            cout << ", bad data record" << (value ? "" : ", no data recovered") << ", record length";
            if (!PrintRecord(value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_data_record_1:
        case simh_class::private_data_record_2:
        case simh_class::private_data_record_3:
        case simh_class::private_data_record_4:
        case simh_class::private_data_record_5:
        case simh_class::private_data_record_6:
            PrintClass(cls);
            cout << ", private data record, record length";
            if (!PrintRecord(value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::tape_description_data_record:
            PrintClass(cls);
            cout << ", tape description data record, record length";
            if (!PrintRecord(value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::reserved_data_record_1:
        case simh_class::reserved_data_record_2:
        case simh_class::reserved_data_record_3:
        case simh_class::reserved_data_record_4:
        case simh_class::reserved_data_record_5:
            PrintClass(cls);
            cout << ", reserved data record, record length";
            if (!PrintRecord(value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_marker:
            PrintClass(cls);
            cout << ", private marker, marker value";
            PrintValue(value);
            break;

        case simh_class::reserved_marker:
            PrintClass(cls);
            if (!PrintReservedMarker(value)) {
                return EXIT_SUCCESS;
            }
            break;

        case simh_class::invalid:
            cerr << "Error: Can't read from '" << filename << "'" << endl;
            return EXIT_FAILURE;

        default:
            assert(false);
            break;
        }
    }

    return EXIT_SUCCESS;
}

void S2pSimh::PrintClass(simh_class cls) const
{
    cout << dec << "Offset " << old_offset << ": Class " << hex << static_cast<int>(cls) << dec;
}

void S2pSimh::PrintValue(int value)
{
    cout << " " << value << " ($" << hex << value << ")\n";

    offset += value;
}

bool S2pSimh::PrintRecord(int value)
{
    PrintValue(value);

    if (dump && limit) {
        vector<uint8_t> record(limit < value ? limit : value);
        file.read((char*)record.data(), record.size());
        if (file.fail()) {
            file.clear();
            cerr << "Can't read record of " << value << " byte(s)" << endl;
            return false;
        }

        cout << FormatBytes(record, static_cast<int>(record.size())) << '\n';
    }

    offset += HEADER_SIZE;

    return true;
}

bool S2pSimh::PrintReservedMarker(int value)
{
    cout << ", reserved marker";

    switch (value) {
    case 0xffffffe:
        cout << " (erase gap)\n";
        break;

    case 0xfffffff:
        cout << " (end of medium)\n";
        return false;

    default:
        cout << ", marker value";
        PrintValue(value);
        break;
    }

    return true;
}
