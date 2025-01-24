//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>
#include "buses/bus.h"

class S2pDumpExecutor;
class SgAdapter;

using namespace std;
using namespace chrono;
using namespace spdlog;

class S2pDump
{

public:

    int Run(span<char*>, bool);

    using ScsiDeviceInfo = struct {
        bool removable;
        byte type;
        byte scsi_level;
        string vendor;
        string product;
        string revision;
        uint32_t sector_size;
        uint64_t capacity;
    };

private:

    void Banner(bool) const;
    bool Init(bool);
    bool ParseArguments(span<char*>);
    void DisplayBoardId() const;
    string ReadWrite(fstream&, int, uint32_t, int, int);
    long CalculateEffectiveSize();
    void ScanBus();
    bool DisplayInquiry(bool);
    bool DisplayScsiInquiry(span<const uint8_t>, bool);
    bool DisplaySasiInquiry(span<const uint8_t>, bool) const;
    void DisplayProperties(int, int) const;
    string DumpRestore();
    string DumpRestoreDisk(fstream&);
    string DumpRestoreTape(fstream&);
    bool GetDeviceInfo();

    void Reset() const;

    void CleanUp() const;
    static void TerminationHandler(int);

    void DumpTape(ostream&);
    void RestoreTape(istream&);

    static void DisplayStatistics(time_point<high_resolution_clock>, uint64_t);

    unique_ptr<Bus> bus;

    shared_ptr<S2pDumpExecutor> s2pdump_executor;

    ScsiDeviceInfo scsi_device_info = { };

    int sasi_capacity = 0;
    int sasi_sector_size = 0;

    vector<uint8_t> buffer;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    bool sasi = false;

    string filename;

    shared_ptr<logger> s2pdump_logger;
    string log_level = "warning";

    int start = 0;
    int count = 0;

    uint64_t byte_count = 0;
    uint32_t block_count = 0;
    uint32_t filemark_count = 0;

    int log_count = 0;

    bool run_inquiry = false;

    bool run_bus_scan = false;

    bool scan_all_luns = false;

    bool restore = false;

    inline static bool active = true;

    string device_file;

#ifdef __linux__
    shared_ptr<SgAdapter> sg_adapter;
#endif

    // Required for the termination handler
    inline static S2pDump *instance;

    static constexpr int MINIMUM_BUFFER_SIZE = 1024 * 64;
    static constexpr int DEFAULT_BUFFER_SIZE = 1024 * 1024;

    static constexpr const char *DIVIDER = "----------------------------------------";

    inline static const unordered_map<byte, const char*> S2P_DEVICE_TYPES = {
        { byte { 0 }, "SCHD" },
        { byte { 1 }, "SCTP" },
        { byte { 2 }, "SCLP" },
        { byte { 3 }, "SCHS" },
        { byte { 5 }, "SCCD" },
        { byte { 7 }, "SCMO" }
    };

    inline static const unordered_map<byte, const char*> SCSI_DEVICE_TYPES = {
        { byte { 0 }, "Direct Access" },
        { byte { 1 }, "Sequential Access" },
        { byte { 2 }, "Printer" },
        { byte { 3 }, "Processor" },
        { byte { 4 }, "Write-Once" },
        { byte { 5 }, "CD-ROM/DVD/BD/DVD-RAM" },
        { byte { 6 }, "Scanner" },
        { byte { 7 }, "Optical Memory" },
        { byte { 8 }, "Media Changer" },
        { byte { 9 }, "Communications" },
        { byte { 10 }, "Graphic Arts Pre-Press" },
        { byte { 11 }, "Graphic Arts Pre-Press" },
        { byte { 12 }, "Storage Array Controller" },
        { byte { 13 }, "Enclosure Services" },
        { byte { 14 }, "Simplified Direct Access" },
        { byte { 15 }, "Optical Card Reader/Writer" },
        { byte { 16 }, "Bridge Controller" },
        { byte { 17 }, "Object-based Storage" },
        { byte { 18 }, "Automation/Drive Interface" },
        { byte { 19 }, "Security Manager" },
        { byte { 20 }, "Host Managed Zoned Block" },
        { byte { 30 }, "Well Known Logical Unit" }
    };

    inline static const string APP_NAME = "s2pdump";
};
