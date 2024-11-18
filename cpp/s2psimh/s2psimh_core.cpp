//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"
#include <cassert>
#include <cstring>
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
        cerr << "Error: Can't open '" << filename << "':" << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    try {
        file_size = filesystem::file_size(filename);
    }
    catch (const filesystem::filesystem_error &e) {
        cerr << "Error: Can't get size of '" << filename << "': " << e.what() << endl;
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
        const int count = ReadHeader(file, header);
        if (count == -1) {
            cerr << "Error: Can't read from '" << filename << "': " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }

        position += count;

        switch (header.cls) {
        case simh_class::tape_mark_good_data_record:
            PrintClass(header);
            if (!header.value) {
                cout << ", tape mark\n";
            }
            else if (!PrintRecord("good data record", header)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::bad_data_record:
            PrintClass(header);
            if (!PrintRecord(header.value ? "bad data record" : "bad data record (no data recovered)", header)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_data_record_1:
        case simh_class::private_data_record_2:
        case simh_class::private_data_record_3:
        case simh_class::private_data_record_4:
        case simh_class::private_data_record_5:
        case simh_class::private_data_record_6:
            PrintClass(header);
            if (!PrintRecord("private data record", header)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::tape_description_data_record:
            PrintClass(header);
            if (!PrintRecord("tape description data record", header)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::reserved_data_record_1:
        case simh_class::reserved_data_record_2:
        case simh_class::reserved_data_record_3:
        case simh_class::reserved_data_record_4:
        case simh_class::reserved_data_record_5:
            PrintClass(header);
            if (!PrintRecord("reserved data record", header)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_marker:
            PrintClass(header);
            cout << ", private marker";
            if ((header.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC && ((header.value >> 24) & 0x0f) == 0b011) {
                cout << " (SCSI2Pi end-of-data object)\n";
                return EXIT_SUCCESS;
            }
            cout << ", marker value";
            PrintValue(header);
            break;

        case simh_class::reserved_marker:
            PrintClass(header);
            if (!PrintReservedMarker(header)) {
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

void S2pSimh::PrintClass(const SimhHeader &header) const
{
    cout << "Offset " << old_position << hex << " ($" << old_position << "): Class " << uppercase
        << static_cast<int>(header.cls) << nouppercase << dec;
}

void S2pSimh::PrintValue(const SimhHeader &header)
{
    cout << " " << header.value << " ($" << hex << header.value << ")\n" << dec;
}

bool S2pSimh::PrintRecord(const string &identifier, const SimhHeader &header)
{
    cout << ", " << identifier << ", record length";

    PrintValue(header);

    const int length = header.value & 0xfffffff;

    if (dump && limit) {
        vector<uint8_t> record(limit < length ? limit : length);
        if (!ReadRecord(record)) {
            cerr << "Error: Can't read record of " << length << " byte(s): " << strerror(errno) << endl;
            return false;
        }

        cout << FormatBytes(record, static_cast<int>(record.size())) << '\n';
    }

    position += length + GetPadding(length);

    array<uint8_t, HEADER_SIZE> data = { };
    file.seekg(position, ios::beg);
    file.read((char*)data.data(), data.size());
    if (const int trailing_length = FromLittleEndian(data); trailing_length != length) {
        cerr << "Error: Trailing record length " << trailing_length << " ($" << hex << trailing_length
            << ") does not match leading length " << dec << length << hex << " ($" << length << ")" << endl;
        return false;
    }

    position += HEADER_SIZE;

    return true;
}

bool S2pSimh::PrintReservedMarker(const simh_util::SimhHeader &header)
{
    cout << ", reserved marker";

    switch (header.value) {
    case 0xffffffe:
        cout << " (erase gap)\n";
        break;

    case 0xfffffff:
        cout << " (end of medium)\n";
        return false;

    default:
        cout << ", marker value";
        PrintValue(header);
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

    return file.good();
}
