//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"
#include <iostream>
#include <filesystem>
#include "shared/s2p_util.h"
#include "shared/simh_util.h"

using namespace s2p_util;
using namespace simh_util;

void S2pSimh::Banner(bool help)
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Simh .tap File Analysis Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2024 Uwe Seimet\n";

    if (help) {
        cout << "Usage: s2psimh <SIMH_TAP_FILE>\n";
    }
}

int S2pSimh::Run(span<char*> args)
{
    Banner(false);

    if (args.size() < 2) {
        return EXIT_FAILURE;
    }

    return Analyze(args[1]);
}

int S2pSimh::Analyze(const string &filename)
{
    ifstream file;

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

    int64_t offset = 0;

    while (offset < file_size) {
        const int current_offset = offset;

        const auto [cls, value] = ReadHeader(file, offset);

        switch (static_cast<simh_class>(cls)) {
        case simh_class::tape_mark_good_data_record:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec;
            if (!value) {
                cout << ", tape mark\n";
            }
            else {
                cout << ", good data record, record length " << value << " ($" << hex << value << ")\n";
            }
            offset += value + (value ? HEADER_SIZE : 0);
            break;

        case simh_class::bad_data_record:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec << ", bad data record"
                << (value ? "" : ", no data recovered") << ", record length " << value << " ($" << hex << value
                << ")\n";
            offset += value + HEADER_SIZE;
            break;

        case simh_class::private_data_record_1:
        case simh_class::private_data_record_2:
        case simh_class::private_data_record_3:
        case simh_class::private_data_record_4:
        case simh_class::private_data_record_5:
        case simh_class::private_data_record_6:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec
                << ", private data record, record length " << value << " ($" << hex << value << ")\n";
            offset += value + HEADER_SIZE;
            break;

        case simh_class::tape_description_data_record:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec
                << ", tape description data record, record length " << value << " ($" << hex << value << ")\n";
            offset += value + HEADER_SIZE;
            break;

        case simh_class::reserved_data_record_1:
        case simh_class::reserved_data_record_2:
        case simh_class::reserved_data_record_3:
        case simh_class::reserved_data_record_4:
        case simh_class::reserved_data_record_5:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec
                << ", reserved data record, record length " << value << " ($" << hex << value << ")\n";
            offset += value + HEADER_SIZE;
            break;

        case simh_class::private_marker:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec
                << ", private marker, marker value " << value << " ($" << hex << value << ")\n";
            offset += value;
            break;

        case simh_class::reserved_marker:
            cout << dec << "Offset " << current_offset << ": Class " << hex << cls << dec << ", reserved marker, ";
            if (value == 0x0fffffff) {
                cout << "end of medium";
                return EXIT_SUCCESS;
            }
            else if (value == 0x0ffffffe) {
                cout << "erase gap";
            }
            else {
                cout << "marker value " << value << " ($" << hex << value << ")";
            }
            cout << '\n';
            break;

        case simh_class::invalid:
            cerr << "Error: Can't read from '" << filename << "'" << endl;
            return EXIT_FAILURE;

        default:
            cout << "Ignored unknown simh class " << hex << cls << dec << ", value " << value << " ($" << hex << value
                << ")\n";
            break;
        }
    }

    return EXIT_SUCCESS;
}
