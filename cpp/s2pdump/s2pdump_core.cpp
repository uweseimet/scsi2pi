//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <spdlog/spdlog.h>
#include <regex>
#include <filesystem>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <array>
#include "controllers/controller_factory.h"
#include "shared/shared_exceptions.h"
#include "shared/s2p_util.h"
#include "s2pdump_core.h"

using namespace std;
using namespace filesystem;
using namespace spdlog;
using namespace scsi_defs;
using namespace s2p_util;

void S2pDump::CleanUp() const
{
    if (bus) {
        bus->CleanUp();
    }
}

void S2pDump::TerminationHandler(int)
{
    instance->bus->SetRST(true);

    instance->CleanUp();

    // Process will terminate automatically
}

bool S2pDump::Banner(span<char*> args) const
{
    if (args.size() > 1 && string(args[1]) == "-v") {
        cout << s2p_util::Banner("(SCSI/SASI Drive Dump/Restore Utility)")
            << "Usage: " << args[0] << "-h ID[:LUN] -i ID[:LUN] [-B BID] [-f FILE] [-a] [-r] [-b BUFFER_SIZE]"
            << " [-L LOG_LEVEL] [-I] [-s] [-t] [-c SASI_CAPACITY] [-z SASI_SECTOR_SIZE] [-S START] [-C COUNT]\n"
            << " ID is the target device ID (0-" << (ControllerFactory::GetIdMax() - 1) << ").\n"
            << " LUN is the optional logical unit, 0 is the default"
            << " (SCSI: 0-" << (ControllerFactory::GetScsiLunMax() - 1)
            << ", SASI: 0-" << (ControllerFactory::GetSasiLunMax() - 1) << ").\n"
            << " The target device can be a SCSI (-i) drive or a SASI (-h) hard drive.\n"
            << " BID is the board ID (0-7). Default is 7.\n"
            << " FILE is the image file path. Only needed when not dumping to stdout.\n"
            << " BUFFER_SIZE is the transfer buffer size in bytes, at least " << MINIMUM_BUFFER_SIZE
            << " bytes. Default is 1 MiB.\n"
            << " SASI_CAPACITY is the capacity of the SASI hard drive in sectors.\n"
            << " SASI_SECTOR_SIZE is the sector size of the SASI hard drive in bytes (256|512|1024).\n"
            << " LOG_LEVEL is the log level (trace|debug|info|warning|error|off), default is 'info'.\n"
            << " -a Scan all potential LUNs during bus scan, default is LUN 0 only.\n"
            << " -r Restore instead of dump.\n"
            << " -S Start sector, default is 0.\n"
            << " -C Sector count, default is the drive capacity.\n"
            << " Only valid for dump and inquiry mode:\n"
            << "   -I Display INQUIRY data of ID[:LUN] and (SCSI only) device properties to be used with"
            << " s2p properties files.\n"
            << "  -s Scan bus for SCSI devices.\n"
            << "  -t Scan bus for SASI devices.\n"
            << flush;

        return false;
    }

    return true;
}

bool S2pDump::Init(bool in_process)
{
    instance = this;
    // Signal handler for cleaning up
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    bus_factory = make_unique<BusFactory>();

    bus = bus_factory->CreateBus(false, in_process);
    if (bus != nullptr) {
        scsi_executor = make_unique<S2pDumpExecutor>(*bus, initiator_id);
    }

    return bus != nullptr;
}

void S2pDump::ParseArguments(span<char*> args)
{
    int buffer_size = DEFAULT_BUFFER_SIZE;

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "b:c:h:i:f:t:z:B:C:L:S:arstI")) != -1) {
        switch (opt) {
        case 'a':
            scan_all_luns = true;
            break;

        case 'b':
            if (!GetAsUnsignedInt(optarg, buffer_size) || buffer_size < MINIMUM_BUFFER_SIZE) {
                throw parser_exception(
                    "Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + " KiB");
            }

            break;

        case 'c':
            if (!GetAsUnsignedInt(optarg, sasi_capacity) || !sasi_capacity) {
                throw parser_exception("Invalid SASI hard drive capacity: " + string(optarg));
            }
            break;

        case 'f':
            filename = optarg;
            break;

        case 'h':
            if (const string error = ProcessId(ControllerFactory::GetIdMax(), ControllerFactory::GetSasiLunMax(),
                optarg, target_id, target_lun); !error.empty()) {
                throw parser_exception(error);
            }
            sasi = true;
            break;

        case 'i':
            if (const string error = ProcessId(ControllerFactory::GetIdMax(), ControllerFactory::GetScsiLunMax(),
                optarg, target_id, target_lun); !error.empty()) {
                throw parser_exception(error);
            }
            break;

        case 'r':
            restore = true;
            break;

        case 's':
            run_bus_scan = true;
            break;

        case 't':
            run_bus_scan = true;
            sasi = true;
            break;

        case 'z':
            if (!GetAsUnsignedInt(optarg, sasi_sector_size)
                || (sasi_sector_size != 256 && sasi_sector_size != 512 && sasi_sector_size != 1024)) {
                throw parser_exception("Invalid SASI hard drive sector size: " + string(optarg));
            }
            break;

        case 'B':
            if (!GetAsUnsignedInt(optarg, initiator_id) || initiator_id > 7) {
                throw parser_exception("Invalid board ID " + string(optarg) + " (0-7)");
            }
            break;

        case 'C':
            if (!GetAsUnsignedInt(optarg, count) || !count) {
                throw parser_exception("Invalid sector count: " + string(optarg));
            }
            break;

        case 'I':
            run_inquiry = true;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'S':
            if (!GetAsUnsignedInt(optarg, start)) {
                throw parser_exception("Invalid start sector: " + string(optarg));
            }
            break;

        default:
            break;
        }
    }

    if (sasi && !sasi_capacity) {
        throw parser_exception("The SASI hard drive capacity (-c) must be provided");
    }

    if (sasi && !sasi_sector_size) {
        throw parser_exception("The SASI hard drive sector_size (-z) must be provided");
    }

    if (target_lun == -1) {
        target_lun = 0;
    }

    if (run_bus_scan) {
        run_inquiry = false;
    }

    buffer = vector<uint8_t>(buffer_size);
}

int S2pDump::Run(span<char*> args, bool in_process)
{
    to_stdout = !isatty(STDOUT_FILENO);

    // Prevent any logging when dumping to stdout
    if (to_stdout) {
        spdlog::set_level(level::off);
    }

    if (!Banner(args)) {
        return EXIT_SUCCESS;
    }

    try {
        ParseArguments(args);

        if (!run_bus_scan && target_id == -1) {
            throw parser_exception("Missing target ID");
        }

        if (target_id == initiator_id) {
            throw parser_exception("Target ID and board ID must not be identical");
        }

        if ((filename.empty() && !run_bus_scan && !run_inquiry && !to_stdout)) {
            throw parser_exception("Missing filename");
        }

        if (!in_process && getuid()) {
            throw parser_exception("GPIO bus access requires root permissions");
        }

        if (!Init(in_process)) {
            throw parser_exception("Can't initialize bus");
        }

        if (!in_process && !bus_factory->IsRaspberryPi()) {
            throw parser_exception("There is no board hardware support");
        }

        if (!to_stdout && !SetLogLevel()) {
            throw parser_exception("Invalid log level '" + log_level + "'");
        }
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (run_bus_scan) {
        ScanBus();
    }
    else if (run_inquiry) {
        DisplayBoardId();

        if (DisplayInquiry(false) && !sasi) {
            DisplayProperties(target_id, target_lun);
        }
    }
    else {
        if (const string error = DumpRestore(); !error.empty()) {
            cerr << "Error: " << error << endl;
            CleanUp();
            return EXIT_FAILURE;
        }
    }

    CleanUp();

    return EXIT_SUCCESS;
}

void S2pDump::DisplayBoardId() const
{
    if (!to_stdout) {
        cout << DIVIDER << "\nBoard ID is " << initiator_id << "\n";
    }
}

void S2pDump::ScanBus()
{
    DisplayBoardId();

    for (target_id = 0; target_id < ControllerFactory::GetIdMax(); target_id++) {
        if (initiator_id == target_id) {
            continue;
        }

        target_lun = 0;
        if (!DisplayInquiry(false) || !scan_all_luns) {
            // Continue with next ID if there is no LUN 0 or only LUN 0 should be scanned
            continue;
        }

        auto luns = scsi_executor->ReportLuns();
        // LUN 0 has already been dealt with
        luns.erase(0);

        for (const auto lun : luns) {
            target_lun = lun;

            DisplayInquiry(false);
        }
    }
}

bool S2pDump::DisplayInquiry(bool check_type)
{
    cout << DIVIDER << "\nChecking " << (sasi ? "SASI" : "SCSI") << " target ID:LUN " << target_id << ":"
        << target_lun << "\n" << flush;

    scsi_executor->SetTarget(target_id, target_lun);

    vector<uint8_t> buf(36);
    if (!scsi_executor->Inquiry(buf, sasi)) {
        return false;
    }

    return sasi ? DisplaySasiInquiry(buf, check_type) : DisplayScsiInquiry(buf, check_type);
}

bool S2pDump::DisplayScsiInquiry(vector<uint8_t> &buf, bool check_type)
{
    scsi_device_info = {};

    scsi_device_info.type = static_cast<byte>(buf[0]);
    if ((scsi_device_info.type & byte { 0x1f }) == byte { 0x1f }) {
        // Requested LUN is not available
        return false;
    }

    array<char, 17> str = { };
    memcpy(str.data(), &buf[8], 8);
    scsi_device_info.vendor = string(str.data());
    cout << "Vendor:               '" << scsi_device_info.vendor << "'\n";

    str.fill(0);
    memcpy(str.data(), &buf[16], 16);
    scsi_device_info.product = string(str.data());
    cout << "Product:              '" << scsi_device_info.product << "'\n";

    str.fill(0);
    memcpy(str.data(), &buf[32], 4);
    scsi_device_info.revision = string(str.data());
    cout << "Revision:             '" << scsi_device_info.revision << "'\n" << flush;

    if (const auto &type = SCSI_DEVICE_TYPES.find(scsi_device_info.type & byte { 0x1f }); type != SCSI_DEVICE_TYPES.end()) {
        cout << "Device Type:          " << (*type).second << "\n";
    }
    else {
        cout << "Device Type:          Unknown\n";
    }

    if (buf[2]) {
        cout << "SCSI Level:           ";
        switch (buf[2]) {
        case 1:
            cout << "SCSI-1-CCS";
            break;

        case 2:
            cout << "SCSI-2";
            break;

        case 3:
            cout << "SCSI-3 (SPC)";
            break;

        default:
            cout << "SPC-" << buf[2] - 2;
            break;
        }
        cout << "\n";
    }

    cout << "Response Data Format: ";
    switch (buf[3]) {
    case 0:
        cout << "SCSI-1";
        break;

    case 1:
        cout << "SCSI-1-CCS";
        break;

    case 2:
        cout << "SCSI-2";
        break;

    default:
        cout << fmt::format("{:02x}", buf[3]);
        break;
    }
    cout << "\n";

    scsi_device_info.removable = (static_cast<byte>(buf[1]) & byte { 0x80 }) == byte { 0x80 };
    cout << "Removable:            " << (scsi_device_info.removable ? "Yes" : "No")
        << "\n";

    if (check_type && scsi_device_info.type != static_cast<byte>(device_type::direct_access) &&
        scsi_device_info.type != static_cast<byte>(device_type::cd_rom)
        && scsi_device_info.type != static_cast<byte>(device_type::optical_memory)) {
        cerr << "Error: Invalid device type for SCSI dump/restore, supported types are DIRECT ACCESS,"
            << " CD-ROM/DVD/BD and OPTICAL MEMORY" << endl;
        return false;
    }

    return true;
}

bool S2pDump::DisplaySasiInquiry(vector<uint8_t> &buf, bool check_type)
{
    const auto type = buf[0];
    if (!type) {
        cout << "Device Type: SASI Hard Drive\n";
    }
    else {
        cout << "Device Type: Unknown\n";
    }

    if (check_type && type) {
        cerr << "Error: Invalid device type for SASI dump/restore, only hard drives are supported" << endl;
        return false;
    }

    return true;
}

string S2pDump::DumpRestore()
{
    if (!GetDeviceInfo()) {
        return "Can't get device information";
    }

    fstream fs;
    if (!to_stdout) {
        fs.open(filename, (restore ? ios::in : ios::out) | ios::binary);
        if (fs.fail()) {
            return "Can't open image file '" + filename + "': " + strerror(errno);
        }
    }

    ostream &out = to_stdout ? cout : fs;

    const auto effective_size = CalculateEffectiveSize();
    if (effective_size < 0) {
        return "";
    }
    if (!effective_size) {
        cerr << "Nothing to do, effective size is 0\n" << flush;
        return "";
    }

    if (!to_stdout) {
        cout << "Starting " << (restore ? "restore" : "dump") << "\n"
            << "  Start sector is " << start << "\n"
            << "  Sector count is " << count << "\n"
            << "  Buffer size is " << buffer.size() << " bytes\n\n"
            << flush;
    }

    const uint32_t sector_size = sasi ? sasi_sector_size : scsi_device_info.sector_size;

    int sector_offset = start;

    auto remaining = effective_size;

    const auto start_time = chrono::high_resolution_clock::now();

    while (remaining) {
        const auto byte_count = static_cast<int>(min(static_cast<size_t>(remaining), buffer.size()));
        auto sector_count = byte_count / sector_size;
        if (byte_count % sector_size) {
            ++sector_count;
        }

        if (sasi && sector_count > 256) {
            sector_count = 256;
        }

        spdlog::debug("Remaining bytes: " + to_string(remaining));
        spdlog::debug("Current sector: " + to_string(sector_offset));
        spdlog::debug("Sector count: " + to_string(sector_count));
        spdlog::debug("Data transfer size: " + to_string(sector_count * sector_size));
        spdlog::debug("Image file chunk size: " + to_string(byte_count));

        if (const string error = ReadWrite(out, fs, sector_offset, sector_count, sector_size, byte_count); !error.empty()) {
            return error;
        }

        sector_offset += sector_count;
        remaining -= byte_count;

        if (!to_stdout) {
            cout << setw(3) << (effective_size - remaining) * 100 / effective_size << "% ("
                << effective_size - remaining
                << "/" << effective_size << " bytes)\n" << flush;
        }
    }

    auto duration = chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now()
        - start_time).count();
    if (!duration) {
        duration = 1;
    }

    if (restore) {
        // Ensure that if the target device is also a SCSI2Pi instance its image file becomes complete immediately
        scsi_executor->SynchronizeCache();
    }

    if (!to_stdout) {
        cout << DIVIDER
            << "\nTransferred " << effective_size / 1024 / 1024 << " MiB (" << effective_size << " bytes)"
            << "\nTotal time: " << duration << " seconds (" << duration / 60 << " minutes)"
            << "\nAverage transfer rate: " << effective_size / duration << " bytes per second ("
            << effective_size / 1024 / duration << " KiB per second)\n"
            << DIVIDER << "\n" << flush;
    }

    return "";
}

string S2pDump::ReadWrite(ostream &out, fstream &fs, int sector_offset, uint32_t sector_count, int sector_size,
    int byte_count)
{
    if (restore) {
        fs.read((char*)buffer.data(), byte_count);
        if (fs.fail()) {
            return "Error reading from file '" + filename + "'";
        }

        if (!scsi_executor->ReadWrite(buffer, sector_offset, sector_count, sector_count * sector_size, true)) {
            return "Error/interrupted while writing to device";
        }
    } else {
        if (!scsi_executor->ReadWrite(buffer, sector_offset, sector_count, sector_count * sector_size, false)) {
            return "Error/interrupted while reading from device";
        }

        out.write((const char*)buffer.data(), byte_count);
        if (out.fail()) {
            return "Error writing to file '" + filename + "'";
        }
    }

    return "";
}

long S2pDump::CalculateEffectiveSize()
{
    const auto capacity = sasi ? sasi_capacity : scsi_device_info.capacity;
    if (capacity <= static_cast<uint64_t>(start)) {
        cerr << "Start sector " << start << " is out of range (" << capacity - 1 << ")" << endl;
        return -1;
    }

    if (!count) {
        count = capacity - start;
    }

    if (capacity < static_cast<uint64_t>(start + count)) {
        cerr << "Sector count " << count << " is out of range (" << capacity - start << ")" << endl;
        return -1;
    }

    const off_t disk_size_in_bytes = count * (sasi ? sasi_sector_size : scsi_device_info.sector_size);

    size_t effective_size;
    if (restore) {
        off_t image_file_size;
        try {
            image_file_size = file_size(path(filename));
        }
        catch (const filesystem_error &e) {
            cerr << "Can't determine image file size: " << e.what() << endl;
            return -1;
        }

        effective_size = min(image_file_size, disk_size_in_bytes);

        if (!to_stdout) {
            cout << "Restore image file size: " << image_file_size << " bytes\n" << flush;
        }

        if (image_file_size > disk_size_in_bytes) {
            cerr << "Warning: Image file size of " << image_file_size
                << " byte(s) is larger than drive size/sector count of " << disk_size_in_bytes << " bytes(s)\n" << flush;
        } else if (image_file_size < disk_size_in_bytes) {
            cerr << "Warning: Image file size of " << image_file_size
                << " byte(s) is smaller than drive size/sector count of " << disk_size_in_bytes << " bytes(s)\n"
                << flush;
        }
    } else {
        effective_size = disk_size_in_bytes;
    }

    return static_cast<long>(effective_size);
}

bool S2pDump::GetDeviceInfo()
{
    DisplayBoardId();

    if (!DisplayInquiry(true)) {
        return false;
    }

    // Clear any pending condition, e.g. a medium just having being inserted
    scsi_executor->TestUnitReady();

    if (!sasi) {
        const auto [capacity, sector_size] = scsi_executor->ReadCapacity();
        if (!capacity || !sector_size) {
            spdlog::trace("Can't read device capacity");
            return false;
        }

        scsi_device_info.capacity = capacity;
        scsi_device_info.sector_size = sector_size;
    }

    uint64_t capacity;
    uint32_t sector_size;
    if (sasi) {
        capacity = sasi_capacity;
        sector_size = sasi_sector_size;
    }
    else {
        capacity = scsi_device_info.capacity;
        sector_size = scsi_device_info.sector_size;
    }

    if (!to_stdout) {
        cout << "Sectors:     " << capacity << "\n"
            << "Sector size: " << sector_size << " bytes\n"
            << "Capacity:    " << sector_size * capacity / 1024 / 1024 << " MiB (" << sector_size * capacity
            << " bytes)\n"
            << DIVIDER << "\n\n"
            << flush;
    }

    return true;
}

void S2pDump::DisplayProperties(int id, int lun) const
{
    cout << "\nDevice properties for s2p properties file:\n";

    string id_and_lun = "device." + to_string(id);
    if (lun > 0) {
        id_and_lun += ":" + to_string(lun);
    }
    id_and_lun += ".";

    cout << id_and_lun << "type=";
    if (const auto &type = S2P_DEVICE_TYPES.find(scsi_device_info.type & byte { 0x1f }); type != S2P_DEVICE_TYPES.end()) {
        if ((*type).second != "SCHD") {
            cout << (*type).second << "\n";
        }
        else {
            cout << (scsi_device_info.removable ? "SCRM" : "SCHD") << "\n";
        }
    }
    else {
        cout << "UNDEFINED\n";
    }

    if (scsi_device_info.sector_size) {
        cout << id_and_lun << "block_size=" << scsi_device_info.sector_size << "\n";
    }

    cout << id_and_lun << "product_name=" << regex_replace(scsi_device_info.vendor, regex(" +$"), "") << ":"
        << regex_replace(scsi_device_info.product, regex(" +$"), "") << ":"
        << regex_replace(scsi_device_info.revision, regex(" +$"), "") << "\n" << flush;

    vector<uint8_t> buf(255);

    if (!scsi_executor->ModeSense6(buf)) {
        cout << "Warning: Can't get mode page data, medium may be missing\n" << flush;
        return;
    }

    const int length = buf[0] + 1;
    int offset = 4;
    while (offset < length) {
        const int page_code = buf[offset++];

        // Mode page 0 has no length field, i.e. its length is the remaining number of bytes
        const int page_length = page_code ? buf[offset++] : length - offset - 1;

        cout << fmt::format("{0}mode_page.{1}={2:02x}", id_and_lun, page_code & 0x3f, page_code);

        if (page_code) {
            cout << fmt::format(":{:02x}", page_length);
        }

        for (int i = 0; i < page_length && offset < length; i++) {
            cout << fmt::format(":{:02x}", buf[offset++]);
        }

        cout << "\n";
    }

    cout << flush;
}

bool S2pDump::SetLogLevel() const
{
    const level::level_enum l = level::from_str(log_level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != log_level) {
        return false;
    }

    set_level(l);

    return true;
}

