//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <getopt.h>
#include <unistd.h>
#include "shared/s2p_util.h"

using namespace filesystem;
using namespace s2p_util;
using namespace simh_util;

void S2pSimh::Banner(bool help)
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (SIMH .tap File Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2024-2026 Uwe Seimet\n";

    if (help) {
        cout << "Usage: s2psimh [options] <SIMH_TAP_FILE>\n"
            << "  --add/-a CLASS1:VALUE1,...    Add objects.\n"
            << "  --binary-data/-b DATA_FILE    Optional binary file to read the record data from.\n"
            << "  --dump/-d                     Dump data record contents.\n"
            << "  --help/-h                     Display this help.\n"
            << "  --hex-data/-x DATA_FILE       Optional text file to read the record data from.\n"
            << "  --limit/-l LIMIT              Limit dump size to LIMIT bytes.\n"
            << "  --truncate/-t                 Truncate file before adding objects.\n"
            << "  --version/-v                  Display the s2psimh version.\n";
    }
}

bool S2pSimh::ParseArguments(span<char*> args)
{
    const vector<option> options = {
        { "add", required_argument, nullptr, 'a' },
        { "binary-data", required_argument, nullptr, 'b' },
        { "dump", no_argument, nullptr, 'd' },
        { "help", no_argument, nullptr, 'h' },
        { "hex-data", required_argument, nullptr, 'x' },
        { "limit", required_argument, nullptr, 'l' },
        { "truncate", no_argument, nullptr, 't' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    bool truncate = false;
    bool version = false;
    bool help = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "-a:b:dhl:tvx:", options.data(), nullptr))
        != -1) {
        switch (opt) {
        case 'a': {
            meta_data = ParseObject(optarg);
            if (meta_data.empty()) {
                return false;
            }
            break;
        }

        case 'b':
            binary_data_filename = optarg;
            break;

        case 'd':
            dump = true;
            break;

        case 'l':
            if (const int l = ParseAsUnsignedInt(string(optarg)); l < 0) {
                cerr << "Error: Invalid dump size limit '" << optarg << "'\n";
                return false;
            }
            else {
                formatter.SetLimit(l);
                limit = static_cast<uint32_t>(l);
            }
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

        case 'x':
            hex_data_filename = optarg;
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

    if (!binary_data_filename.empty() && !hex_data_filename.empty()) {
        cerr << "Error: There can only be a single data file\n";
        return false;
    }

    if (truncate) {
        ofstream f(simh_filename);
        if (!f) {
            cerr << "Error: Can't open '" << simh_filename << "'\n";
            return false;
        }
        f.close();

        if (::truncate(simh_filename.c_str(), 0) == -1) {
            cerr << "Error: Can't truncate '" << simh_filename << "'\n";
            return false;
        }
    }

    return true;
}

int S2pSimh::Run(span<char*> args)
{
    if (!ParseArguments(args)) {
        return EXIT_SUCCESS;
    }

    simh_file.open(simh_filename, (meta_data.empty() ? ios::in : ios::out) | ios::binary);
    if (!simh_file) {
        cerr << "Error: Can't open '" << simh_filename << "':" << strerror(errno) << '\n';
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
        simh_file_size = file_size(simh_filename);
    }
    catch (const filesystem_error &e) {
        cerr << "Error: Can't get size of '" << simh_filename << "': " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    while (position < simh_file_size) {
        old_position = position;

        simh_file.seekg(position);

        SimhMetaData meta;
        if (!ReadMetaData(simh_file, meta)) {
            cerr << "Error: Can't read from '" << simh_filename << "': " << strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        position += META_DATA_SIZE;

        switch (meta.cls) {
        case SimhClass::TAPE_MARK_GOOD_DATA_RECORD:
            PrintClass(meta);
            if (!meta.value) {
                cout << ", tape mark\n";
            }
            else if (!PrintRecord("good data record", meta)) {
                return EXIT_FAILURE;
            }
            break;

        case SimhClass::BAD_DATA_RECORD:
            PrintClass(meta);
            if (!PrintRecord(meta.value ? "bad data record" : "bad data record (no data recovered)",
                meta)) {
                return EXIT_FAILURE;
            }
            break;

        case SimhClass::PRIVATE_DATA_RECORD_1:
        case SimhClass::PRIVATE_DATA_RECORD_2:
        case SimhClass::PRIVATE_DATA_RECORD_3:
        case SimhClass::PRIVATE_DATA_RECORD_4:
        case SimhClass::PRIVATE_DATA_RECORD_5:
        case SimhClass::PRIVATE_DATA_RECORD_6:
            PrintClass(meta);
            if (!PrintRecord("private data record", meta)) {
                return EXIT_FAILURE;
            }
            break;

        case SimhClass::TAPE_DESCRIPTION_DATA_RECORD:
            PrintClass(meta);
            if (!PrintRecord("tape description data record", meta)) {
                return EXIT_FAILURE;
            }
            break;

        case SimhClass::RESERVED_DATA_RECORD_1:
        case SimhClass::RESERVED_DATA_RECORD_2:
        case SimhClass::RESERVED_DATA_RECORD_3:
        case SimhClass::RESERVED_DATA_RECORD_4:
        case SimhClass::RESERVED_DATA_RECORD_5:
            PrintClass(meta);
            if (!PrintRecord("reserved data record", meta)) {
                return EXIT_FAILURE;
            }
            break;

        case SimhClass::PRIVATE_MARKER:
            PrintClass(meta);
            cout << ", private marker";
            if ((meta.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC && ((meta.value >> 24) & 0x0f) == 0b011) {
                cout << " (SCSI2Pi end-of-data object)\n";
                return EXIT_SUCCESS;
            }
            cout << ", marker value";
            PrintValue(meta);
            break;

        case SimhClass::RESERVERD_MARKER:
            PrintClass(meta);
            if (!PrintReservedMarker(meta)) {
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
    const bool has_data_file = !binary_data_filename.empty() || !hex_data_filename.empty();
    const bool binary = !binary_data_filename.empty();
    const string &data_filename = binary ? binary_data_filename : hex_data_filename;

    vector<byte> input_data;

    off_t filesize = 0;

    if (has_data_file) {
        try {
            filesize = file_size(data_filename);
        }
        catch (const filesystem_error &e) {
            cerr << "Error: Can't get size of '" << data_filename + "': " << e.what() << '\n';
            return EXIT_FAILURE;
        }

        data_file.open(data_filename, ios::in);
        if (!data_file.is_open()) {
            cerr << "Error: Can't read from '" << data_filename << "': " << strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        if (binary) {
            input_data.resize(filesize);
            data_file.read(reinterpret_cast<char*>(input_data.data()), input_data.size());
        }
        else {
            string line;
            while (getline(data_file, line)) {
                vector<byte> bytes;
                try {
                    bytes = HexToBytes(line);
                }
                catch (const out_of_range&)
                {
                    cerr << "Error: Invalid input data format: '" + line + "'\n";
                }

                copy(bytes.begin(), bytes.end(), back_inserter(input_data));
            }

            filesize = input_data.size();
        }

        if (data_file.bad()) {
            cerr << "Error: Can't read from '" << data_filename << "': " << strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        data_file.close();
    }

    simh_file.seekp(0, ios::end);

    size_t data_index = 0;

    for (const auto &object : meta_data) {
        const auto &data = ToLittleEndian(object);
        simh_file.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (simh_file.bad()) {
            cerr << "Can't write to '" << simh_filename << "': " << strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        if (IsRecord(object) && !(object.cls == SimhClass::BAD_DATA_RECORD && !object.value)) {
            const uint32_t length = object.value & 0x0fffffff;
            if (has_data_file) {
                if (data_index >= input_data.size()) {
                    cerr << "Error: Not enough record data in '" << data_filename << "'\n";
                    return EXIT_FAILURE;
                }
                simh_file.write(reinterpret_cast<const char*>(input_data.data()) + data_index, length);
                data_index += length;
            }
            else {
                simh_file.seekp(Pad(length), ios::cur);
            }

            if (length != Pad(length)) {
                simh_file << '\0';
            }

            simh_file.write(reinterpret_cast<const char*>(data.data()), data.size());

            if (simh_file.bad()) {
                cerr << "Can't write to '" << simh_filename << "': " << strerror(errno) << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

void S2pSimh::PrintClass(const SimhMetaData &meta) const
{
    cout << "Offset " << old_position << hex << "/$" << old_position << ": Class " << uppercase
        << static_cast<int>(meta.cls) << nouppercase << dec;
}

void S2pSimh::PrintValue(const SimhMetaData &meta)
{
    cout << " " << meta.value << " ($" << hex << meta.value << ")\n" << dec;
}

bool S2pSimh::PrintRecord(const string &identifier, const SimhMetaData &meta)
{
    cout << ", " << identifier;

    if (meta.cls == SimhClass::BAD_DATA_RECORD && !meta.value) {
        cout << '\n';
        return true;
    }

    cout << ", record length";

    PrintValue(meta);

    if (dump && limit) {
        vector<uint8_t> record(limit < meta.value ? limit : meta.value);
        if (!ReadRecord(record)) {
            cerr << "Error: Can't read record of " << meta.value << " byte(s)\n";
            return false;
        }

        cout << formatter.FormatBytes(record, record.size()) << '\n';
    }

    position += Pad(meta.value);

    array<uint8_t, META_DATA_SIZE> data = { };
    simh_file.seekg(position);
    simh_file.read(reinterpret_cast<char*>(data.data()), data.size());
    if (simh_file.bad()) {
        return false;
    }

    if (const uint32_t trailing_length = FromLittleEndian(data).value; trailing_length != meta.value) {
        cerr << "Error: Trailing record length " << trailing_length << " ($" << hex << trailing_length
            << ") at offset " << dec << position << " does not match leading length " << meta.value << hex
            << " ($" << meta.value << ")\n";
        return false;
    }

    position += META_DATA_SIZE;

    return true;
}

bool S2pSimh::PrintReservedMarker(const simh_util::SimhMetaData &meta)
{
    cout << ", reserved marker";

    switch (meta.value) {
    case 0x0ffffffe:
        cout << " (erase gap)\n";
        break;

    case 0x0fffffff:
        cout << " (end of medium)\n";
        return false;

    default:
        cout << ", marker value";
        PrintValue(meta);
        break;
    }

    return true;
}

bool S2pSimh::ReadRecord(span<uint8_t> buf)
{
    if (static_cast<off_t>(position + buf.size()) > simh_file_size) {
        return false;
    }

    simh_file.read(reinterpret_cast<char*>(buf.data()), buf.size());

    return simh_file.good();
}

vector<SimhMetaData> S2pSimh::ParseObject(const string &s)
{
    vector<SimhMetaData> objects;

    for (const auto &object : Split(s, ',')) {
        const auto &components = Split(object, ':');
        if (components.empty() || components.size() % 2) {
            cerr << "Error: Invalid class/value definition '" << object << "'\n";
            return {};
        }

        const string &cls = ToLower(components[0]);
        const int c = HexToDec(cls[0]);
        if (cls.size() > 1 || c == -1) {
            cerr << "Error: Invalid class '" << cls << "'\n";
            return {};
        }

        const string &value = components[1];
        if (const int v = ParseAsUnsignedInt(value); v < 0) {
            cerr << "Error: Invalid value '" << value << "'\n";
            return {};
        }
        else {
            objects.push_back(SimhMetaData( { static_cast<SimhClass>(c), static_cast<uint32_t>(v) }));
        }
    }

    return objects;
}
