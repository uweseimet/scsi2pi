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
#include <unistd.h>
#include "shared/s2p_util.h"

using namespace s2p_util;
using namespace simh_util;

void S2pSimh::Banner(bool help)
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (SIMH .tap File Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2024 Uwe Seimet\n";

    if (help) {
        cout << "Usage: s2psimh [options] <SIMH_TAP_FILE>\n"
            << "  --add/-a CLASS1:VALUE1,...  Add objects.\n"
            << "  --data DATA_FILE            Optional file to read the record data from.\n"
            << "  --dump/-d                   Dump data record contents.\n"
            << "  --limit/-l LIMIT            Limit dump size to LIMIT bytes.\n"
            << "  --truncate/-t               Truncate file before adding objects.\n"
            << "  --version/-v                Display the program version.\n"
            << "  --help/-h                   Display this help.\n";
    }
}

bool S2pSimh::ParseArguments(span<char*> args)
{
    const int OPT_DATA = 2;

    const vector<option> options = {
        { "add", required_argument, nullptr, 'a' },
        { "data", required_argument, nullptr, OPT_DATA },
        { "dump", no_argument, nullptr, 'd' },
        { "limit", required_argument, nullptr, 'l' },
        { "truncate", no_argument, nullptr, 't' },
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    bool truncate = false;
    bool version = false;
    bool help = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "-a:dhl:tv", options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'a': {
            meta_data = ParseObject(optarg);
            if (meta_data.empty()) {
                return false;
            }
        }
            break;

        case 'd':
            dump = true;
            break;

        case 'l':
            int l;
            if (!GetAsUnsignedInt(string(optarg), l)) {
                cerr << "Error: Invalid dump size limit " << optarg << endl;
                return false;
            }
            limit = static_cast<uint32_t>(l);
            break;

        case 'h':
            help = true;
            break;

        case 't':
            truncate = true;
            break;

        case 'v':
            version = true;
            break;

        case 1:
            simh_filename = optarg;
            break;

        case OPT_DATA:
            data_filename = optarg;
            break;

        default:
            Banner(false);
            return false;
        }

        if (!simh_filename.empty()) {
            break;
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

    if (simh_filename.empty()) {
        Banner(true);
        return false;
    }

    if (truncate) {
        ofstream f(simh_filename);
        if (f.fail()) {
            cerr << "Error: Can't open '" << simh_filename << "'" << endl;
            return false;
        }
        f.close();
        ::truncate(simh_filename.c_str(), 0);
    }

    return true;
}

int S2pSimh::Run(span<char*> args)
{
    if (!ParseArguments(args)) {
        return EXIT_SUCCESS;
    }

    simh_file.open(simh_filename, ios::in | ios::out | ios::binary);
    if (simh_file.fail()) {
        cerr << "Error: Can't open '" << simh_filename << "':" << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    if (meta_data.empty()) {
        return Analyze();
    }

    return Add();
}

int S2pSimh::Analyze()
{
    try {
        simh_file_size = filesystem::file_size(simh_filename);
    }
    catch (const filesystem::filesystem_error &e) {
        cerr << "Error: Can't get size of '" << simh_filename << "': " << e.what() << endl;
        return EXIT_FAILURE;
    }

    while (position < simh_file_size) {
        old_position = position;

        simh_file.seekg(position);

        SimhMetaData meta_data;
        if (!ReadMetaData(simh_file, meta_data)) {
            cerr << "Error: Can't read from '" << simh_filename << "': " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }

        position += META_DATA_SIZE;

        switch (meta_data.cls) {
        case simh_class::tape_mark_good_data_record:
            PrintClass(meta_data);
            if (!meta_data.value) {
                cout << ", tape mark\n";
            }
            else if (!PrintRecord("good data record", meta_data)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::bad_data_record:
            PrintClass(meta_data);
            if (!PrintRecord(meta_data.value ? "bad data record" : "bad data record (no data recovered)",
                meta_data)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_data_record_1:
        case simh_class::private_data_record_2:
        case simh_class::private_data_record_3:
        case simh_class::private_data_record_4:
        case simh_class::private_data_record_5:
        case simh_class::private_data_record_6:
            PrintClass(meta_data);
            if (!PrintRecord("private data record", meta_data)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::tape_description_data_record:
            PrintClass(meta_data);
            if (!PrintRecord("tape description data record", meta_data)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::reserved_data_record_1:
        case simh_class::reserved_data_record_2:
        case simh_class::reserved_data_record_3:
        case simh_class::reserved_data_record_4:
        case simh_class::reserved_data_record_5:
            PrintClass(meta_data);
            if (!PrintRecord("reserved data record", meta_data)) {
                return EXIT_FAILURE;
            }
            break;

        case simh_class::private_marker:
            PrintClass(meta_data);
            cout << ", private marker";
            if ((meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC && ((meta_data.value >> 24) & 0x0f) == 0b011) {
                cout << " (SCSI2Pi end-of-data object)\n";
                return EXIT_SUCCESS;
            }
            cout << ", marker value";
            PrintValue(meta_data);
            break;

        case simh_class::reserved_marker:
            PrintClass(meta_data);
            if (!PrintReservedMarker(meta_data)) {
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

int S2pSimh::Add()
{
    if (!data_filename.empty()) {
        data_file.open(data_filename, ios::in);
        if (!data_file.is_open()) {
            cerr << "Error: Can't read from '" << data_filename << "': " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    simh_file.seekp(0, ios::end);

    for (const auto &object : meta_data) {
        const auto &data = ToLittleEndian(object);
        simh_file.write((const char*)data.data(), data.size());
        if (simh_file.bad()) {
            cerr << "Can't write to '" << simh_filename << "': " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }

        if (IsRecord(object) && !(object.cls == simh_class::bad_data_record && !object.value)) {
            const uint32_t length = object.value & 0x0fffffff;

            if (data_file.is_open()) {
                vector<uint8_t> record_data(length);
                data_file.read((char*)record_data.data(), record_data.size());
                if (data_file.bad()) {
                    cerr << "Error: Can't read from '" << data_filename << "': " << strerror(errno) << endl;
                    return EXIT_FAILURE;
                }
                if (data_file.eof()) {
                    cerr << "Error: Not enough record data in '" << data_filename << "'" << endl;
                    return EXIT_FAILURE;
                }
                simh_file.write((const char*)record_data.data(), record_data.size());
            }
            else {
                simh_file.seekp(Pad(length), ios::cur);
            }

            if (length != Pad(length)) {
                simh_file << '\0';
            }

            simh_file.write((const char*)data.data(), data.size());

            if (simh_file.bad()) {
                cerr << "Can't write to '" << simh_filename << "': " << strerror(errno) << endl;
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

void S2pSimh::PrintClass(const SimhMetaData &meta_data) const
{
    cout << "Offset " << old_position << hex << " ($" << old_position << "): Class " << uppercase
        << static_cast<int>(meta_data.cls) << nouppercase << dec;
}

void S2pSimh::PrintValue(const SimhMetaData &meta_data)
{
    cout << " " << meta_data.value << " ($" << hex << meta_data.value << ")\n" << dec;
}

bool S2pSimh::PrintRecord(const string &identifier, const SimhMetaData &meta_data)
{
    cout << ", " << identifier;

    if (meta_data.cls == simh_class::bad_data_record && !meta_data.value) {
        cout << '\n';
        return true;
    }

    cout << ", record length";

    PrintValue(meta_data);

    if (dump && limit) {
        vector<uint8_t> record(limit < meta_data.value ? limit : meta_data.value);
        if (!ReadRecord(record)) {
            cerr << "Error: Can't read record of " << meta_data.value << " byte(s): " << strerror(errno) << endl;
            return false;
        }

        cout << FormatBytes(record, static_cast<int>(record.size())) << '\n';
    }

    position += Pad(meta_data.value);

    array<uint8_t, META_DATA_SIZE> data = { };
    simh_file.seekg(position);
    simh_file.read((char*)data.data(), data.size());
    if (const uint32_t trailing_length = FromLittleEndian(data).value; trailing_length != meta_data.value) {
        cerr << "Error: Trailing record length " << trailing_length << " ($" << hex << trailing_length
            << ") does not match leading length " << dec << meta_data.value << hex << " ($" << meta_data.value << ")"
            << endl;
        return false;
    }

    position += META_DATA_SIZE;

    return true;
}

bool S2pSimh::PrintReservedMarker(const simh_util::SimhMetaData &meta_data)
{
    cout << ", reserved marker";

    switch (meta_data.value) {
    case 0x0ffffffe:
        cout << " (erase gap)\n";
        break;

    case 0x0fffffff:
        cout << " (end of medium)\n";
        return false;

    default:
        cout << ", marker value";
        PrintValue(meta_data);
        break;
    }

    return true;
}

bool S2pSimh::ReadRecord(span<uint8_t> buf)
{
    if (static_cast<off_t>(position + buf.size()) > simh_file_size) {
        return false;
    }

    simh_file.read((char*)buf.data(), buf.size());

    return simh_file.good();
}

vector<SimhMetaData> S2pSimh::ParseObject(const string &s)
{
    vector<SimhMetaData> objects;

    for (const auto &object : Split(s, ',')) {
        const auto &components = Split(object, ':');
        if (components.empty() || components.size() % 2) {
            cerr << "Error: Invalid class/value definition '" << object << "'" << endl;
            return {};
        }

        const string &cls = ToLower(components[0]);
        const int c = HexToDec(cls[0]);
        if (cls.size() > 1 || c == -1) {
            cerr << "Error: Invalid class '" << cls << "'" << endl;
            return {};
        }

        const string &value = components[1];
        int v;
        if (!GetAsUnsignedInt(value, v) || v > 0xffffffff) {
            cerr << "Error: Invalid value '" << value << "'" << endl;
            return {};
        }

        objects.push_back(SimhMetaData(static_cast<simh_class>(c), static_cast<uint32_t>(v)));
    }

    return objects;
}
