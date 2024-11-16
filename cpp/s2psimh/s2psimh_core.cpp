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
        cout << "Usage: s2psimh [options] <SIMH_TAP_FILE>\n"
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

    try {
        file_size = filesystem::file_size(filename);
    }
    catch (const filesystem::filesystem_error &e) {
        cerr << "Error: Can't get size of '" << filename << "': " << e.what();
        return EXIT_FAILURE;
    }

    return Analyze();
}

int S2pSimh::Analyze()
{
    while (position < file_size) {
        old_position = position;

        file.seekg(position, ios::beg);

        SimhHeader header;
        const int count = ReadHeader( { file, file_size }, header);
        if (count == -1) {
            cerr << "Error: Can't read from '" << filename << "'" << endl;
            return EXIT_FAILURE;
        }

        position += count;

        switch (header.cls) {
        case simh_class::tape_mark_good_data_record:
            PrintClass(header.cls);
            if (!header.value) {
                cout << ", tape mark\n";
            }
            else {
                if (!PrintRecord("good data record", header.value)) {
                    return EXIT_FAILURE;
                }
            }
            break;

        case simh_class::bad_data_record:
            PrintClass(header.cls);
            if (!PrintRecord(header.value ? "bad data record" : "bad data record (no data recovered)", header.value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_data_record_1:
        case simh_class::private_data_record_2:
        case simh_class::private_data_record_3:
        case simh_class::private_data_record_4:
        case simh_class::private_data_record_5:
        case simh_class::private_data_record_6:
            PrintClass(header.cls);
            if (!PrintRecord("private data record", header.value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::tape_description_data_record:
            PrintClass(header.cls);
            if (!PrintRecord("tape description data record", header.value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::reserved_data_record_1:
        case simh_class::reserved_data_record_2:
        case simh_class::reserved_data_record_3:
        case simh_class::reserved_data_record_4:
        case simh_class::reserved_data_record_5:
            PrintClass(header.cls);
            if (!PrintRecord("reserved data record", header.value)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_marker:
            PrintClass(header.cls);
            cout << ", private marker";
            // SCSI2Pi end-of-data?
            if (static_cast<int>(header.value == (PRIVATE_MARKER_MAGIC | 0b011))) {
                cout << " (SCSI2Pi end-of-data object)\n";
                return EXIT_SUCCESS;
            }
            cout << ", marker value";
            PrintValue(header.value);
            break;

        case simh_class::reserved_marker:
            PrintClass(header.cls);
            if (!PrintReservedMarker(header.value)) {
                return EXIT_SUCCESS;
            }
            break;

        default:
            assert(false);
            break;
        }
    }

    return EXIT_SUCCESS;
}

void S2pSimh::PrintClass(simh_class cls) const
{
    cout << dec << "Offset " << old_position << ": Class " << hex << static_cast<int>(cls) << dec;
}

void S2pSimh::PrintValue(int value)
{
    cout << " " << value << " ($" << hex << value << ")\n";
}

bool S2pSimh::PrintRecord(const string &identifier, int value)
{
    cout << ", " << identifier << ", record length";

    PrintValue(value);

    const int length = value & 0xfffffff;

    if (dump && limit) {
        file.seekg(position, ios::beg);

        vector<uint8_t> record(limit < length ? limit : length);
        if (!ReadRecord(record)) {
            cerr << "Error: Can't read record of " << length << " byte(s)" << endl;
            return false;
        }

        cout << FormatBytes(record, static_cast<int>(record.size())) << '\n';
    }

    position += length + GetPadding(length) + HEADER_SIZE;

    array<uint8_t, HEADER_SIZE> data = { };
    file.seekg(position - HEADER_SIZE, ios::beg);
    file.read((char*)data.data(), data.size());
    const int trailing_length = FromLittleEndian(data);
    if (length != trailing_length) {
        cerr << "Error: Trailing record length " << trailing_length << " ($" << hex << trailing_length
            << ") does not match leading length " << dec << length << hex << " ($" << length << ")" << endl;
        return false;
    }

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

bool S2pSimh::ReadRecord(span<uint8_t> buf)
{
    if (static_cast<off_t>(position + buf.size()) > file_size) {
        return false;
    }

    file.read((char*)buf.data(), buf.size());
    if (file.fail()) {
        return false;
    }

    return true;
}
