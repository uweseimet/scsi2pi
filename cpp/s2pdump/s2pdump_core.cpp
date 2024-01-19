//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <array>
#include <regex>
#include <getopt.h>
#include <spdlog/spdlog.h>
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

void S2pDump::Banner(bool header) const
{
    if (header) {
        cout << s2p_util::Banner("(Drive Dump/Restore Utility)");
    }

    cout << "Usage: s2pdump [options]\n"
        << "  --scsi-id/-i ID[:LUN]              SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                                     default LUN is 0.\n"
        << "  --sasi-id/-h ID[:LUN]              SASI target device ID (0-7) and LUN (0-1),\n"
        << "                                     default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID             Board (initiator) ID (0-7), default is 7.\n"
        << "  --image-file/-f IMAGE_FILE         Image file path.\n"
        << "  --buffer-size/-b BUFFER_SIZE       Transfer buffer size, at least " << MINIMUM_BUFFER_SIZE << " bytes,"
        << "                                     default is 1 MiB.\n"
        << "  --log-level/-L LOG_LEVEL           Log level (trace|debug|info|warning|\n"
        << "                                     error|off), default is 'info'.\n"
        << "  --inquiry/-I                       Display INQUIRY data and (SCSI only)\n"
        << "                                     device properties for s2p property files.\n"
        << "  --scsi-scan/-s                     Scan bus for SCSI devices.\n"
        << "  --sasi-scan/-t                     Scan bus for SASI devices.\n"
        << "  --sasi-capacity/-c CAPACITY        SASI drive capacity in sectors.\n"
        << "  --sasi-sector-size/-z SECTOR_SIZE  SASI drive sector size (256|512|1024).\n"
        << "  --start-sector/-S START            Start sector, default is 0.\n"
        << "  --sector-count/-C COUNT            Sector count, default is the capacity.\n"
        << "  --all-luns/-a                      Check all LUNs during bus scan,\n"
        << "                                     default is LUN 0 only.\n"
        << "  --restore/-r                       Restore instead of dump.\n";
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

bool S2pDump::ParseArguments(span<char*> args)
{
    const vector<option> options = {
        { "all-luns", no_argument, nullptr, 'a' },
        { "buffer-size", required_argument, nullptr, 'b' },
        { "board-id", required_argument, nullptr, 'B' },
        { "help", no_argument, nullptr, 'H' },
        { "sasi-capacity", required_argument, nullptr, 'c' },
        { "sector-count", required_argument, nullptr, 'C' },
        { "sasi-id", required_argument, nullptr, 'h' },
        { "scsi-id", required_argument, nullptr, 'i' },
        { "inquiry", no_argument, nullptr, 'I' },
        { "image-file", required_argument, nullptr, 'f' },
        { "log-level", required_argument, nullptr, 'L' },
        { "restore", no_argument, nullptr, 'r' },
        { "scsi-scan", no_argument, nullptr, 's' },
        { "start-sector", required_argument, nullptr, 'S' },
        { "sasi-scan", no_argument, nullptr, 't' },
        { "sasi-sector-size", required_argument, nullptr, 'z' },
        { nullptr, 0, nullptr, 0 }
    };

    string buf;
    string initiator = "7";
    string id_and_lun;
    string start_sector;
    string sector_count;
    string capacity;
    string sector_size;
    int buffer_size = DEFAULT_BUFFER_SIZE;
    bool scsi = false;
    bool version = false;
    bool help = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "ab:B:c:C:h:Hi:If:L:rsS:tvz:", options.data(),
        nullptr)) != -1) {
        switch (opt) {
        case 'a':
            scan_all_luns = true;
            break;

        case 'b':
            buf = optarg;
            break;

        case 'B':
            initiator = optarg;
            break;

        case 'c':
            capacity = optarg;
            break;

        case 'C':
            sector_count = optarg;
            break;

        case 'f':
            filename = optarg;
            break;

        case 'h':
            id_and_lun = optarg;
            sasi = true;
            break;

        case 'H':
            help = true;
            break;

        case 'i':
            id_and_lun = optarg;
            scsi = true;
            break;

        case 'I':
            run_inquiry = true;
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'r':
            restore = true;
            break;

        case 's':
            run_bus_scan = true;
            scsi = true;
            break;

        case 'S':
            start_sector = optarg;
            break;

        case 't':
            run_bus_scan = true;
            sasi = true;
            break;

        case 'v':
            version = true;
            break;

        case 'z':
            sector_size = optarg;
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

    if (!SetLogLevel()) {
        throw parser_exception("Invalid log level '" + log_level + "'");
    }

    if (scsi && sasi) {
        throw parser_exception("SCSI and SASI devices cannot be mixed");
    }

    if (!GetAsUnsignedInt(initiator, initiator_id) || initiator_id > 7) {
        throw parser_exception("Invalid board ID '" + initiator + "' (0-7)");
    }

    if (!run_bus_scan) {
        if (const string error = ProcessId(8, sasi ? 2 : 32, id_and_lun, target_id, target_lun); !error.empty()) {
            throw parser_exception(error);
        }

        if (!buf.empty()) {
            if (!GetAsUnsignedInt(buf, buffer_size) || buffer_size < MINIMUM_BUFFER_SIZE) {
                throw parser_exception(
                    "Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + " KiB");
            }
        }

        if (!sector_count.empty()) {
            if (!GetAsUnsignedInt(sector_count, count) || !count) {
                throw parser_exception("Invalid sector count: '" + sector_count + "'");
            }
        }

        if (!start_sector.empty()) {
            if (!GetAsUnsignedInt(start_sector, start)) {
                throw parser_exception("Invalid start sector: " + string(optarg));
            }
        }

        if (sasi) {
            if (!GetAsUnsignedInt(capacity, sasi_capacity) || !sasi_capacity) {
                throw parser_exception("Invalid SASI hard drive capacity: '" + capacity + "'");
            }

            if (!GetAsUnsignedInt(sector_size, sasi_sector_size)
                || (sasi_sector_size != 256 && sasi_sector_size != 512 && sasi_sector_size != 1024)) {
                throw parser_exception("Invalid SASI hard drive sector size: '" + sector_size + "'");
            }
        }

        if (target_id == -1) {
            throw parser_exception("Missing target ID");
        }

        if (target_id == initiator_id) {
            throw parser_exception("Target ID and board ID must not be identical");
        }

        if (filename.empty() && !run_bus_scan && !run_inquiry) {
            throw parser_exception("Missing filename");
        }

        if (target_lun == -1) {
            target_lun = 0;
        }
    }
    else {
        run_inquiry = false;
    }

    buffer = vector<uint8_t>(buffer_size);

    return true;
}

int S2pDump::Run(span<char*> args, bool in_process)
{
    if (args.size() < 2) {
        Banner(true);
        return EXIT_FAILURE;
    }

    try {
        if (!ParseArguments(args)) {
            return EXIT_SUCCESS;
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
    cout << DIVIDER << "\nBoard (initiator) ID is " << initiator_id << "\n";
}

void S2pDump::ScanBus()
{
    DisplayBoardId();

    for (target_id = 0; target_id < 8; target_id++) {
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

    fstream fs(filename, (restore ? ios::in : ios::out) | ios::binary);
    if (fs.fail()) {
        return "Can't open image file '" + filename + "': " + strerror(errno);
    }

    const auto effective_size = CalculateEffectiveSize();
    if (effective_size < 0) {
        return "";
    }
    if (!effective_size) {
        cerr << "Nothing to do, effective size is 0\n" << flush;
        return "";
    }

    cout << "Starting " << (restore ? "restore" : "dump") << "\n"
        << "  Start sector is " << start << "\n"
        << "  Sector count is " << count << "\n"
        << "  Buffer size is " << buffer.size() << " bytes\n\n"
        << flush;

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

        if (const string error = ReadWrite(fs, sector_offset, sector_count, sector_size, byte_count); !error.empty()) {
            return error;
        }

        sector_offset += sector_count;
        remaining -= byte_count;

        cout << setw(3) << (effective_size - remaining) * 100 / effective_size << "% ("
            << effective_size - remaining
            << "/" << effective_size << " bytes)\n" << flush;
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

    cout << DIVIDER
        << "\nTransferred " << effective_size / 1024 / 1024 << " MiB (" << effective_size << " bytes)"
        << "\nTotal time: " << duration << " seconds (" << duration / 60 << " minutes)"
        << "\nAverage transfer rate: " << effective_size / duration << " bytes per second ("
        << effective_size / 1024 / duration << " KiB per second)\n"
        << DIVIDER << "\n" << flush;

    return "";
}

string S2pDump::ReadWrite(fstream &fs, int sector_offset, uint32_t sector_count, int sector_size, int byte_count)
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

        fs.write((const char*)buffer.data(), byte_count);
        if (fs.fail()) {
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

        cout << "Restore image file size: " << image_file_size << " bytes\n" << flush;

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

    cout << "Sectors:     " << capacity << "\n"
        << "Sector size: " << sector_size << " bytes\n"
        << "Capacity:    " << sector_size * capacity / 1024 / 1024 << " MiB (" << sector_size * capacity
        << " bytes)\n"
        << DIVIDER << "\n\n"
        << flush;

    return true;
}

void S2pDump::DisplayProperties(int id, int lun) const
{
    cout << "\nDevice properties for s2p property file:\n";

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
        const int page_length = page_code ? buf[offset++] : length - offset;

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

