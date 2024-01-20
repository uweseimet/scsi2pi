//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <unordered_map>
#include "buses/bus_factory.h"
#include "s2pdump_executor.h"

using namespace std;

class S2pDump
{

public:

    int Run(span<char*>, bool = false);

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
    string ReadWrite(fstream&, int, uint32_t, int, int);
    long CalculateEffectiveSize();
    void ScanBus();
    bool DisplayInquiry(bool);
    bool DisplayScsiInquiry(vector<uint8_t>&, bool);
    bool DisplaySasiInquiry(vector<uint8_t>&, bool) const;
    void DisplayProperties(int, int) const;
    string DumpRestore();
    bool GetDeviceInfo();

    bool SetLogLevel() const;

    void Reset() const;

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<BusFactory> bus_factory;

    unique_ptr<Bus> bus;

    unique_ptr<S2pDumpExecutor> scsi_executor;

    scsi_device_info_t scsi_device_info;

    int sasi_capacity = 0;
    int sasi_sector_size = 0;

    vector<uint8_t> buffer;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    bool sasi = false;

    string filename;

    string log_level = "info";

    int start = 0;
    int count = 0;

    bool run_inquiry = false;

    bool run_bus_scan = false;

    bool scan_all_luns = false;

    bool restore = false;

    // Required for the termination handler
    static inline S2pDump *instance;

    static const int MINIMUM_BUFFER_SIZE = 1024 * 64;
    static const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

    static inline const string DIVIDER = "----------------------------------------";

    static inline const unordered_map<byte, string> S2P_DEVICE_TYPES = {
        { byte { 0 }, "SCHD" },
        { byte { 2 }, "SCLP" },
        { byte { 3 }, "SCHS" },
        { byte { 5 }, "SCCD" },
        { byte { 7 }, "SCMO" }
    };

    static inline const unordered_map<byte, string> SCSI_DEVICE_TYPES = {
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
