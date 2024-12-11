//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <unordered_map>
#include <vector>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "disk_executor.h"
#include "tape_executor.h"

using namespace std;
using namespace chrono;

class S2pDump
{

public:

    int Run(span<char*>, bool);

    struct scsi_device_info
    {
        bool removable;
        byte type;
        byte scsi_level;
        string vendor;
        string product;
        string revision;
        uint32_t sector_size;
        uint64_t capacity;
    };
    using scsi_device_info_t = struct scsi_device_info;

private:

    void Banner(bool) const;
    bool Init(bool);
    bool ParseArguments(span<char*>);
    void DisplayBoardId() const;
    string ReadWriteDisk(fstream&, int, uint32_t, int, int);
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

    unique_ptr<DiskExecutor> disk_executor;
    unique_ptr<TapeExecutor> tape_executor;

    scsi_device_info_t scsi_device_info = { };

    int sasi_capacity = 0;
    int sasi_sector_size = 0;

    vector<uint8_t> buffer;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    bool sasi = false;

    string filename;

    shared_ptr<logger> initiator_logger = stdout_color_mt("initiator");
    string log_level = "warning";

    int start = 0;
    int count = 0;

    uint64_t byte_count = 0;
    uint32_t block_count = 0;
    uint32_t filemark_count = 0;

    bool run_inquiry = false;

    bool run_bus_scan = false;

    bool scan_all_luns = false;

    bool restore = false;

    // Required for the termination handler
    static inline S2pDump *instance;

    static constexpr int MINIMUM_BUFFER_SIZE = 1024 * 64;
    static constexpr int DEFAULT_BUFFER_SIZE = 1024 * 1024;

    static constexpr const char *DIVIDER = "----------------------------------------";

    static inline const unordered_map<byte, const char*> S2P_DEVICE_TYPES = {
        { byte { 0 }, "SCHD" },
        { byte { 1 }, "SCTP" },
        { byte { 2 }, "SCLP" },
        { byte { 3 }, "SCHS" },
        { byte { 5 }, "SCCD" },
        { byte { 7 }, "SCMO" }
    };

    static inline const unordered_map<byte, const char*> SCSI_DEVICE_TYPES = {
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
};
