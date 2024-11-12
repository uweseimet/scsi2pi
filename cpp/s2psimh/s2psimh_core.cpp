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
            cout << "Offset " << current_offset << ": Class " << cls << ", value " << value
                << (value ? " (data record)" : " (tape mark) ") << '\n';
            offset += value + (value ? HEADER_SIZE : 0);
            break;

        case simh_class::reserved_marker:
            cout << "Offset " << current_offset << ": Class " << cls << ", value " << value << '\n';
            break;

        case simh_class::invalid:
            cerr << "Error: Can't read from '" << filename << "'" << endl;
            return EXIT_FAILURE;

        default:
            cout << "Error: Ignored unknown simh class " << cls << ", value " << value << '\n';
            break;
        }
    }

    return EXIT_SUCCESS;
}
