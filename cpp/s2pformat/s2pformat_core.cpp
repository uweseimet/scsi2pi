//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pformat_core.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <getopt.h>
#include <spdlog/spdlog.h>
#include "initiator/initiator_util.h"
#include "shared/memory_util.h"
#include "shared/s2p_util.h"

using namespace memory_util;
using namespace s2p_util;
using namespace initiator_util;

void S2pFormat::Banner(bool header) const
{
    if (header) {
        cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Format Tool)\n"
            << "Version " << GetVersionString() << "\n"
            << "Copyright (C) 2024 Uwe Seimet\n";
    }

    cout << "Usage: s2pformat [options] </dev/sg*>\n"
        << "  --log-level/-L LEVEL  Log level (trace|debug|info|warning|error|\n"
        << "                        critical|off), default is 'info'.\n"
        << "  --version/-v          Display the program version.\n"
        << "  --help/-H             Display this help.\n";
}

bool S2pFormat::ParseArguments(span<char*> args) // NOSONAR Acceptable complexity for parsing
{
    const vector<option> options = {
        { "help", no_argument, nullptr, 'H' },
        { "log-level", required_argument, nullptr, 'L' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    bool version = false;
    bool help = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "-hvL:", options.data(),
        nullptr)) != -1) {
        switch (opt) {
        case 'h':
            help = true;
            break;

        case 'L':
            if (!SetLogLevel(*default_logger(), optarg)) {
                cerr << "Invalid log level '" << optarg << "'" << endl;
                return false;
            }
            break;

        case 'v':
            version = true;
            break;

        case 1:
            device = optarg;
            break;

        default:
            Banner(true);
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

    return true;
}

int S2pFormat::Run(span<char*> args)
{
    if (args.size() < 2) {
        Banner(true);
        return EXIT_FAILURE;
    }

    if (!ParseArguments(args)) {
        return EXIT_SUCCESS;
    }

    sg_adapter = make_unique<SgAdapter>(*default_logger());

    if (const string &error = sg_adapter->Init(device); !error.empty()) {
        cerr << "Error: " << error << endl;
        return EXIT_FAILURE;
    }

    const auto &descriptors = GetFormatDescriptors();

    const int n = SelectFormat(descriptors);
    if (!n) {
        sg_adapter->CleanUp();
        return EXIT_SUCCESS;
    }

    cout << "Are you sure? Formatting will erase all data and may take long (N/y)\n";

    string input;
    getline(cin, input);

    if (input == "y") {
        if (const string &error = Format(descriptors, n); !error.empty()) {
            cerr << "Error: " << error << endl;
            sg_adapter->CleanUp();
            return EXIT_FAILURE;
        }
    }

    sg_adapter->CleanUp();

    return EXIT_SUCCESS;
}

vector<S2pFormat::FormatDescriptor> S2pFormat::GetFormatDescriptors()
{
    vector<uint8_t> buf(36);
    vector<uint8_t> cdb(6);

    if (ExecuteCommand(cdb, { }, 3)) {
        cerr << "Error: Can't get drive data: " << strerror(errno) << endl;
        return {};
    }

    cdb[0] = static_cast<uint8_t>(scsi_command::inquiry);
    cdb[4] = static_cast<uint8_t>(buf.size());
    if (ExecuteCommand(cdb, buf, 3)) {
        cerr << "Error: Can't get drive data: " << strerror(errno) << endl;
        return {};
    }

    array<char, 17> str = { };
    memcpy(str.data(), &buf[8], 8);
    cout << "Vendor:   '" << str.data() << "'\n";

    str.fill(0);
    memcpy(str.data(), &buf[16], 16);
    cout << "Product:  '" << str.data() << "'\n";

    str.fill(0);
    memcpy(str.data(), &buf[32], 4);
    cout << "Revision: '" << str.data() << "'\n";

    cdb.resize(10);
    buf.resize(252);
    cdb[4] = 0;
    cdb[0] = static_cast<uint8_t>(scsi_command::read_format_capacities);
    cdb[8] = static_cast<uint8_t>(buf.size());

    if (ExecuteCommand(cdb, buf, 5)) {
        return {};
    }

    cout << "Current number of sectors: " << GetInt32(buf, 4)
        << "\nCurrent sector size: " << GetInt24(buf, 9) << '\n';

    vector<FormatDescriptor> descriptors;

    for (auto i = 12; i < buf[3]; i += 8) {
        // Ignore other format types than 0
        if (!(buf[i + 4] & 0x03)) {
            descriptors.push_back( { GetInt32(buf, i), GetInt32(buf, i + 4) & 0xffffff });
        }
    }

    return descriptors;
}

int S2pFormat::SelectFormat(span<const S2pFormat::FormatDescriptor> descriptors)
{
    cout << "Formats supported by this drive:\n";

    int n = 1;
    for (const auto &descriptor : descriptors) {
        cout << "  " << n << ". " << descriptor.blocks << " sectors, " << descriptor.length << " bytes per sector\n";
        ++n;
    }

    cout << "Select a format, press Enter without input to quit\n";

    string input;
    getline(cin, input);
    n = 0;
    try {
        n = stoi(input);
    }
    catch (const invalid_argument&)
    {
        // Fall through
    }

    if (n <= 0 || n > static_cast<int>(descriptors.size())) {
        return 0;
    }

    const auto &descriptor = descriptors[n - 1];

    cout << "Format the drive with " << descriptor.blocks << " sectors, " << descriptor.length
        << " bytes per sector? (N/y)\n";

    getline(cin, input);

    return input == "y" ? n : 0;
}

string S2pFormat::Format(span<const S2pFormat::FormatDescriptor> descriptors, int n)
{
    vector<uint8_t> cdb(6);
    vector<uint8_t> parameters;

    cdb[0] = static_cast<uint8_t>(scsi_command::format_unit);
    if (n) {
        // FmdData
        cdb[1] = 0x17;

        parameters.resize(12);
        SetInt16(parameters, 2, 8);
        SetInt32(parameters, 4, descriptors[n - 1].blocks);
        SetInt32(parameters, 8, descriptors[n - 1].length);
    }

    const int status = ExecuteCommand(cdb, parameters, 3600);

    return status ? fmt::format("Can't format drive: {}", strerror(errno)) : "";
}

int S2pFormat::ExecuteCommand(span<uint8_t> cdb, span<uint8_t> buf, int timeout)
{
    return sg_adapter->SendCommand(cdb, buf, static_cast<int>(buf.size()), timeout).status;
}
