//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pdump_core.h"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <regex>
#include <getopt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "buses/bus_factory.h"
#include "initiator/initiator_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/simh_util.h"
#include "board_executor.h"
#ifdef __linux__
#include "shared/sg_adapter.h"
#include "sg_executor.h"
#endif

using namespace filesystem;
using namespace s2p_util;
using namespace initiator_util;
using namespace simh_util;

void S2pDump::CleanUp() const
{
    if (bus) {
        bus->CleanUp();
    }
}

void S2pDump::TerminationHandler(int)
{
    if (instance) {
        instance->bus->SetRST(true);

        instance->CleanUp();
    }

    active = false;
}

void S2pDump::Banner(bool header) const
{
    if (header) {
        cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Hard Drive/Tape Drive Dump/Restore Tool)\n"
            << "Version " << GetVersionString() << "\n"
            << "Copyright (C) 2023-2025 Uwe Seimet\n";
    }

    cout << "Usage: " + APP_NAME + " [options]\n"
        << "  --scsi-id/-i ID[:LUN]              SCSI target device ID (0-7) and LUN (0-31),\n"
        << "                                     default LUN is 0.\n"
        << "  --sasi-id/-h ID[:LUN]              SASI target device ID (0-7) and LUN (0-1),\n"
        << "                                     default LUN is 0.\n"
        << "  --board-id/-B BOARD_ID             Board (initiator) ID (0-7), default is 7.\n"
        << "  --image-file/-f IMAGE_FILE         Source/Destination image file path.\n"
        << "  --buffer-size/-b BUFFER_SIZE       Transfer buffer size, at least " << MINIMUM_BUFFER_SIZE << " bytes,"
        << "                                     default is 1 MiB.\n"
        << "  --log-level/-L LOG_LEVEL           Log level (trace|debug|info|warning|\n"
        << "                                     error|critical|off), default is 'warning'.\n"
        << "  --inquiry/-I                       Display INQUIRY data and (SCSI only)\n"
        << "                                     device properties for property files.\n"
        << "  --retries/-R                       Number of disk drive retries, default is 0.\n"
        << "  --scsi-scan/-s                     Scan bus for SCSI devices.\n"
        << "  --sasi-scan/-t                     Scan bus for SASI devices.\n"
        << "  --sasi-capacity/-c CAPACITY        SASI drive capacity in sectors.\n"
        << "  --sasi-sector-size/-z SECTOR_SIZE  SASI drive sector size (256|512|1024).\n"
        << "  --start-sector/-S START            Hard drive start sector, default is 0.\n"
        << "  --sector-count/-C COUNT            Hard drive sector count,\n"
        << "                                     default is the capacity.\n"
        << "  --all-luns/-a                      Check all LUNs during bus scan,\n"
        << "                                     default is LUN 0 only.\n"
        << "  --restore/-r                       Restore instead of dump.\n"
        << "  --scsi-generic/-g DEVICE_FILE      Use the Linux SG driver instead of a\n"
        << "                                     RaSCSI/PiSCSI board.\n"
        << "  --version/-v                       Display the program version.\n"
        << "  --help/-H                          Display this help.\n";
}

bool S2pDump::Init(bool in_process)
{
    bus = bus_factory::CreateBus(false, in_process, APP_NAME, false);
    if (!bus) {
        return false;
    }

    instance = this;
    // Signal handler for cleaning up
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    return true;
}

bool S2pDump::ParseArguments(span<char*> args) // NOSONAR Acceptable complexity for parsing
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
        { "scsi-generic", required_argument, nullptr, 'g' },
        { "inquiry", no_argument, nullptr, 'I' },
        { "image-file", required_argument, nullptr, 'f' },
        { "log-level", required_argument, nullptr, 'L' },
        { "restore", no_argument, nullptr, 'r' },
        { "retries", required_argument, nullptr, 'R' },
        { "scsi-scan", no_argument, nullptr, 's' },
        { "start-sector", required_argument, nullptr, 'S' },
        { "sasi-scan", no_argument, nullptr, 't' },
        { "sasi-sector-size", required_argument, nullptr, 'z' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string buf;
    string initiator;
    string id_and_lun;
    string retry_count;
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
    while ((opt = getopt_long(static_cast<int>(args.size()), args.data(), "ab:B:c:C:g:h:Hi:If:L:rR:sS:tvz:",
        options.data(),
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

        case 'g':
            device_file = optarg;
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

        case 'R':
            retry_count = optarg;
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

    if (!SetLogLevel(*s2pdump_logger, log_level)) {
        throw ParserException("Invalid log level '" + log_level + "'");
    }

    if (scsi && sasi) {
        throw ParserException("SCSI and SASI functionality cannot be mixed");
    }

    if (initiator.empty() && device_file.empty()) {
        initiator = "7";
    }

    if ((!initiator.empty() && !device_file.empty()) || (!device_file.empty() && !id_and_lun.empty())) {
        throw ParserException("Either a RaSCSI/PiSCSI board or the Linux SG driver can be used");
    }

    if (!initiator.empty()) {
        if (initiator_id = ParseAsUnsignedInt(initiator); initiator_id < 0 || initiator_id > 7) {
            throw ParserException("Invalid initiator ID '" + initiator + "' (0-7)");
        }
    }

#ifdef __linux__
    if (!device_file.empty()) {
        sg_adapter = make_shared<SgAdapter>(*s2pdump_logger);
        if (const string &error = sg_adapter->Init(device_file); !error.empty()) {
            throw ParserException(error);
        }
    }
#endif

    if (!run_bus_scan) {
        if (device_file.empty()) {
            if (const string &error = ParseIdAndLun(id_and_lun, target_id, target_lun); !error.empty()) {
                throw ParserException(error);
            }
        }

        if (!buf.empty()) {
            if (buffer_size = ParseAsUnsignedInt(buf); buffer_size < MINIMUM_BUFFER_SIZE) {
                throw ParserException(
                    "Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + " KiB");
            }
        }

        if (!sector_count.empty()) {
            if (count = ParseAsUnsignedInt(sector_count); count <= 0) {
                throw ParserException("Invalid sector count: '" + sector_count + "'");
            }
        }

        if (!start_sector.empty()) {
            if (start = ParseAsUnsignedInt(start_sector); start < 0) {
                throw ParserException("Invalid start sector: " + string(optarg));
            }
        }

        if (!retry_count.empty()) {
            if (retries = ParseAsUnsignedInt(retry_count); retries < 0) {
                throw ParserException("Invalid retry count: " + string(optarg));
            }
        }

        if (sasi) {
            if (sasi_capacity = ParseAsUnsignedInt(capacity); sasi_capacity <= 0) {
                throw ParserException("Invalid SASI hard drive capacity: '" + capacity + "'");
            }

            if (sasi_sector_size = ParseAsUnsignedInt(sector_size); sasi_sector_size != 256 && sasi_sector_size != 512
                && sasi_sector_size != 1024) {
                throw ParserException("Invalid SASI hard drive sector size: '" + sector_size + "'");
            }
        }

        if (device_file.empty() && target_id == -1) {
            throw ParserException("Missing target ID");
        }

        if (target_id == initiator_id) {
            throw ParserException("Target ID and initiator ID must not be identical");
        }

        if (filename.empty() && !run_bus_scan && !run_inquiry) {
            throw ParserException("Missing filename");
        }

        // Avoid -1 as target ID
        if (!device_file.empty()) {
            target_id = 0;
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

    s2pdump_logger = CreateLogger(APP_NAME);

    try {
        if (!ParseArguments(args)) {
            return EXIT_SUCCESS;
        }

        if (device_file.empty()) {
            if (!Init(in_process)) {
                throw ParserException("Can't initialize bus");
            }

            if (!in_process && !bus->IsRaspberryPi()) {
                throw ParserException("There is no board hardware support");
            }
        }
    }
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

#ifdef __linux__
    if (device_file.empty()) {
        s2pdump_executor = make_shared<BoardExecutor>(*bus, initiator_id, *s2pdump_logger);
    }
    else {
        s2pdump_executor = make_shared<SgExecutor>(*sg_adapter, *s2pdump_logger);
    }
#else
    s2pdump_executor = make_shared<BoardExecutor>(*bus, initiator_id, *s2pdump_logger);
#endif

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
        if (const string &error = DumpRestore(); !error.empty()) {
            cerr << "Error: " << error << '\n';
            CleanUp();
            return EXIT_FAILURE;
        }
    }

    CleanUp();

    return EXIT_SUCCESS;
}

void S2pDump::DisplayBoardId() const
{
    if (device_file.empty()) {
        cout << DIVIDER << "\nBoard (initiator) ID is " << initiator_id << "\n";
    }
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

        auto luns = s2pdump_executor->ReportLuns();
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
    if (device_file.empty()) {
        cout << DIVIDER << "\nChecking " << (sasi ? "SASI" : "SCSI") << " target ID:LUN " << target_id << ":"
            << target_lun << "\n" << flush;
    }
    else {
        cout << "Checking device corresponding to Linux SG driver device file '" << device_file << "'\n" << flush;
    }

    if (auto board_executor = dynamic_pointer_cast<BoardExecutor>(s2pdump_executor); board_executor) {
        board_executor->SetTarget(target_id, target_lun, sasi);
    }

    // Clear potential UNIT ATTENTION status
    s2pdump_executor->TestUnitReady();

    vector<uint8_t> buf(36);
    if (!s2pdump_executor->Inquiry(buf)) {
        return false;
    }

    return sasi ? DisplaySasiInquiry(buf, check_type) : DisplayScsiInquiry(buf, check_type);
}

bool S2pDump::DisplayScsiInquiry(span<const uint8_t> buf, bool check_type)
{
    const auto type = static_cast<int>(buf[0]) & 0x0f;
    if (type == 0x1f) {
        // Requested LUN is not available
        return false;
    }

    cout << "\nINQUIRY product data:\n";

    scsi_device_info = { };
    scsi_device_info.type = static_cast<byte>(buf[0]);

    const auto& [vendor, product, revision] = GetInquiryProductData(buf);
    scsi_device_info.vendor = vendor;
    scsi_device_info.product = product;
    scsi_device_info.revision = revision;
    cout << "Vendor:               '" << scsi_device_info.vendor << "'\n";
    cout << "Product:              '" << scsi_device_info.product << "'\n";
    cout << "Revision:             '" << scsi_device_info.revision << "'\n";

    if (const auto &t = SCSI_DEVICE_TYPES.find(static_cast<byte>(type)); t != SCSI_DEVICE_TYPES.end()) {
        cout << "Device Type:          " << (*t).second << "\n";
    }
    else {
        cout << "Device Type:          Unknown\n";
    }

    cout << "SCSI Level:           " << GetScsiLevel(buf[2]) << '\n';

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

    scsi_device_info.removable = (static_cast<int>(buf[1]) & 0x80) == 0x80;
    cout << "Removable:            " << (scsi_device_info.removable ? "Yes" : "No")
        << "\n";

    if (check_type && scsi_device_info.type != static_cast<byte>(DeviceType::DIRECT_ACCESS) &&
        scsi_device_info.type != static_cast<byte>(DeviceType::CD_DVD)
        && scsi_device_info.type != static_cast<byte>(DeviceType::OPTICAL_MEMORY)
        && scsi_device_info.type != static_cast<byte>(DeviceType::SEQUENTIAL_ACCESS)) {
        cerr << "Error: Invalid device type for SCSI dump/restore, supported types are DIRECT ACCESS,"
            << " CD-ROM/DVD/BD/DVD-RAM, OPTICAL MEMORY and SEQUENTIAL ACCESS\n";
        return false;
    }

    return true;
}

bool S2pDump::DisplaySasiInquiry(span<const uint8_t> buf, bool check_type) const
{
    const auto type = buf[0];
    if (!type) {
        cout << "Device Type: SASI Hard Drive\n";
    }
    else {
        cout << "Device Type: Unknown\n";
    }

    if (check_type && type) {
        cerr << "Error: Invalid device type for SASI dump/restore, only hard drives are supported\n";
        return false;
    }

    return true;
}

string S2pDump::DumpRestore()
{
    if (!GetDeviceInfo()) {
        return "Can't get device information";
    }

    fstream file(filename, (restore ? ios::in : ios::out) | ios::binary);
    if (!file) {
        return "Can't open image file '" + filename + "': " + strerror(errno);
    }

    if (!restore) {
        permissions(filename, perms::group_read | perms::group_write | perms::others_read | perms::others_write,
            perm_options::add);
    }

    return
        scsi_device_info.type == static_cast<byte>(DeviceType::SEQUENTIAL_ACCESS) ?
            DumpRestoreTape(file) : DumpRestoreDisk(file);
}

string S2pDump::DumpRestoreDisk(fstream &file)
{
    const auto effective_size = CalculateEffectiveSize();
    if (effective_size < 0) {
        return "";
    }
    if (!effective_size) {
        cerr << "Nothing to do, effective size is 0\n" << flush;
        return "";
    }

    cout << "Starting " << (restore ? "restore from '" : "dump to '") << filename << "'\n"
        << "  Start sector is " << start << "\n"
        << "  Sector count is " << count << "\n"
        << "  Buffer size is " << buffer.size() << " bytes\n\n"
        << flush;

    const uint32_t sector_size = sasi ? sasi_sector_size : scsi_device_info.sector_size;

    int sector_offset = start;

    auto remaining = effective_size;

    const auto start_time = high_resolution_clock::now();

    while (remaining && active) {
        auto current_count = static_cast<int>(min(static_cast<size_t>(remaining), buffer.size()));
        auto sector_count = current_count / sector_size;
        if (current_count % sector_size) {
            ++sector_count;
        }

        if (sasi && sector_count > 256) {
            sector_count = 256;
            current_count = sector_count * sector_size;
        }

        s2pdump_logger->info("Remaining bytes: {}", remaining);
        s2pdump_logger->info("Current sector: {}", sector_offset);
        s2pdump_logger->info("Sector count: {}", sector_count);
        s2pdump_logger->info("Data transfer size: {}", sector_count * sector_size);
        s2pdump_logger->info("Image file chunk size: {}", current_count);

        if (const string &error = ReadWriteWithRetry(file, sector_offset, sector_count, sector_size, current_count); !error.empty()) {
            return error;
        }

        sector_offset += sector_count;
        remaining -= current_count;

        cout << setw(3) << (effective_size - remaining) * 100 / effective_size << "% ("
            << effective_size - remaining
            << "/" << effective_size << " bytes)\n" << flush;
    }

    if (restore) {
        // Ensure that if the target device is also a SCSI2Pi instance its image file becomes complete immediately
        s2pdump_executor->SynchronizeCache();
    }

    DisplayStatistics(start_time, effective_size);

    return "";
}

string S2pDump::ReadWriteWithRetry(fstream &file, int sector_offset, int sector_count, int sector_size,
    int current_count)
{
    int r = 0;
    while (true) {
        const string &error = ReadWrite(file, sector_offset, sector_count, sector_size, current_count);
        if (error.empty()) {
            return "";
        }

        if (r == retries) {
            return error;
        }

        ++r;
    }
}

string S2pDump::DumpRestoreTape(fstream &file)
{
    cout << "Rewinding tape\n";
    if (s2pdump_executor->Rewind()) {
        return "Can't rewind tape";
    }

    cout << "Starting " << (restore ? "restore from '" : "dump to '") << filename << "'\n";

    const auto start_time = high_resolution_clock::now();

    try {
        restore ? RestoreTape(file) : DumpTape(file);
    }
    catch (const IoException &e) {
        return e.what();
    }

    DisplayStatistics(start_time, byte_count);

    return "";
}

string S2pDump::ReadWrite(fstream &file, int sector_offset, uint32_t sector_count, int sector_size, int byte_count)
{
    if (restore) {
        file.read((char*)buffer.data(), byte_count);
        if (file.fail()) {
            return "Can't read from file '" + filename + "': " + strerror(errno);
        }

        if (!s2pdump_executor->ReadWrite(buffer, sector_offset, sector_count, sector_count * sector_size, true)) {
            return "Can't write to device: " + string(strerror(errno));
        }
    } else {
        if (!s2pdump_executor->ReadWrite(buffer, sector_offset, sector_count, sector_count * sector_size, false)) {
            return "Can't read from device: " + string(strerror(errno));
        }

        file.write((const char*)buffer.data(), byte_count);
        if (file.fail()) {
            return "Can't write to file '" + filename + "': " + strerror(errno);
        }
    }

    return "";
}

void S2pDump::DumpTape(ostream &file)
{
    while (active) {
        const int length = s2pdump_executor->ReadWrite(buffer, 0);
        if (length == BoardExecutor::NO_MORE_DATA) {
            break;
        }

        if (length == BoardExecutor::BAD_BLOCK) {
            const array<uint8_t, 4> bad_data = { 0x00, 0x00, 0x00, 0x80 };
            file.write((const char*)bad_data.data(), bad_data.size());
            if (file.bad()) {
                throw IoException("Can't write SIMH bad data record");
            }
        }
        else if (length) {
            if (!WriteGoodData(file, buffer, length)) {
                throw IoException("Can't write SIMH good data record");
            }

            ++block_count;
            byte_count += length;

            log_count += length;
        }
        else {
            if (!WriteFilemark(file)) {
                throw IoException("Can't write SIMH tape mark");
            }

            ++filemark_count;
        }

        s2pdump_logger->info("Byte count: {}", byte_count);
        s2pdump_logger->info("Block count: {}", block_count);
        s2pdump_logger->info("Filemark count: {}", filemark_count);

        if (log_count >= 131072) {
            cout << "Dumped " << byte_count << " bytes (" << byte_count / 1048576 << " MB)\n" << flush;
            log_count = 0;
        }
    }
}

void S2pDump::RestoreTape(istream &file)
{
    while (true) {
        SimhMetaData meta_data;
        if (!ReadMetaData(file, meta_data)) {
            break;
        }

        if (meta_data.cls == SimhClass::RESERVERD_MARKER
            && meta_data.value == static_cast<uint32_t>(SimhMarker::END_OF_MEDIUM)) {
            break;
        }

        // Tape mark
        if (meta_data.cls == SimhClass::TAPE_MARK_GOOD_DATA_RECORD && !meta_data.value) {
            s2pdump_logger->debug("Writing filemark");

            if (s2pdump_executor->WriteFilemark()) {
                throw IoException("Can't write filemark");
            }

            ++filemark_count;
        }
        else if ((meta_data.cls == SimhClass::TAPE_MARK_GOOD_DATA_RECORD
            || meta_data.cls == SimhClass::BAD_DATA_RECORD) && meta_data.value) {
            s2pdump_logger->debug("Writing {} byte(s) block", meta_data.value);

            buffer.resize(meta_data.value);

            file.read((char*)buffer.data(), buffer.size());
            if (file.bad()) {
                throw IoException("Can't read SIMH data record");
            }

            if (s2pdump_executor->ReadWrite(buffer, meta_data.value) != static_cast<int>(meta_data.value)) {
                throw IoException("Can't write block");
            }

            file.seekg(META_DATA_SIZE, ios::cur);

            ++block_count;
            byte_count += buffer.size();

            log_count += buffer.size();
        }

        s2pdump_logger->info("Byte count: {}", byte_count);
        s2pdump_logger->info("Block count: {}", block_count);
        s2pdump_logger->info("Filemark count: {}", filemark_count);

        if (log_count >= 131072) {
            cout << "Restored " << byte_count << " bytes (" << byte_count / 1048576 << " MB)\n" << flush;
            log_count = 0;
        }
    }
}

long S2pDump::CalculateEffectiveSize()
{
    const auto capacity = sasi ? sasi_capacity : scsi_device_info.capacity;
    if (capacity <= static_cast<uint64_t>(start)) {
        cerr << "Start sector " << start << " is out of range (" << capacity - 1 << ")\n";
        return -1;
    }

    if (!count) {
        count = static_cast<int>(capacity - start);
    }

    if (capacity < static_cast<uint64_t>(start + count)) {
        cerr << "Sector count " << count << " is out of range (" << capacity - start << ")\n";
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
            cerr << "Can't determine image file size: " << e.what() << '\n';
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
    if (!device_file.empty()) {
        DisplayBoardId();
    }

    if (!DisplayInquiry(true)) {
        return false;
    }

    // Clear any pending error condition, e.g. a medium just having being inserted
    s2pdump_executor->RequestSense( { });

    if (scsi_device_info.type == static_cast<byte>(DeviceType::SEQUENTIAL_ACCESS)) {
        return true;
    }

    if (!sasi) {
        const auto [capacity, sector_size] = s2pdump_executor->ReadCapacity();
        if (!capacity || !sector_size) {
            trace("Can't read device capacity");
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
    // Clear any pending error condition, e.g. a medium just having being inserted
    s2pdump_executor->RequestSense( { });

    cout << "\nDevice properties for s2p properties file:\n";

    string id_and_lun = "device." + to_string(id);
    if (lun > 0) {
        id_and_lun += ":" + to_string(lun);
    }
    id_and_lun += ".";

    cout << id_and_lun << "type=";
    const auto type = static_cast<int>(scsi_device_info.type);
    if (const auto &t = S2P_DEVICE_TYPES.find(static_cast<byte>(type & 0x1f)); t != S2P_DEVICE_TYPES.end()) {
        if (string((*t).second) != "SCHD") {
            cout << (*t).second << "\n";
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

    cout << id_and_lun << "name=" << regex_replace(scsi_device_info.vendor, regex(" +$"), "") << ":"
        << regex_replace(scsi_device_info.product, regex(" +$"), "") << ":"
        << regex_replace(scsi_device_info.revision, regex(" +$"), "") << "\n" << flush;

    vector<uint8_t> buf(255);

    if (!s2pdump_executor->ModeSense6(buf)) {
        cout << "Warning: No mode page data available, medium might be missing\n" << flush;
        return;
    }

    const int length = buf[0] + 1;
    int offset = 4;
    while (offset < length) {
        const int page_code = buf[offset];
        ++offset;

        // Mode page 0 has no length field, i.e. its length is the remaining number of bytes
        const int page_length = page_code ? buf[offset] : length - offset;

        cout << fmt::format("{0}mode_page.{1}={2:02x}", id_and_lun, page_code & 0x3f, page_code);

        if (page_code) {
            cout << fmt::format(":{:02x}", page_length);

            ++offset;
        }

        for (int i = 0; i < page_length && offset < length; i++, offset++) {
            cout << fmt::format(":{:02x}", buf[offset]);
        }

        cout << "\n";
    }

    cout << flush;
}

void S2pDump::DisplayStatistics(time_point<high_resolution_clock> start_time, uint64_t count)
{
    auto duration = duration_cast<chrono::seconds>(high_resolution_clock::now() - start_time).count();
    if (!duration) {
        duration = 1;
    }

    cout << DIVIDER
        << "\nTransferred " << count / 1024 / 1024 << " MiB (" << count << " bytes)"
        << "\nTotal time: " << duration << " seconds (" << duration / 60 << " minutes)"
        << "\nAverage transfer rate: " << count / duration << " bytes per second ("
        << count / 1024 / duration << " KiB per second)\n"
        << DIVIDER << "\n" << flush;
}
