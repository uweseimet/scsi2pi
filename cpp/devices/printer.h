//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// Implementation of a SCSI printer (see SCSI-2 specification for a command description)
//
//---------------------------------------------------------------------------
#pragma once

#include <fstream>
#include <unordered_map>
#include "base/interfaces/scsi_printer_commands.h"
#include "base/primary_device.h"

using namespace std;

class Printer : public PrimaryDevice, private ScsiPrinterCommands
{

public:

    explicit Printer(int);
    ~Printer() override = default;

    bool Init(const param_map&) override;
    void CleanUp() override;

    param_map GetDefaultParams() const override;

    vector<uint8_t> InquiryInternal() const override;

    int WriteData(span<const uint8_t>, scsi_command) override;

    vector<PbStatistics> GetStatistics() const override;

private:

    void TestUnitReady() override;
    void Print() override;
    void SynchronizeBuffer();

    string file_template;

    string filename;

    ofstream out;

    uint64_t file_print_count = 0;
    uint64_t byte_receive_count = 0;
    uint64_t print_error_count = 0;
    uint64_t print_warning_count = 0;

    static constexpr int NOT_RESERVED = -2;

    static constexpr const char *PRINTER_FILE_PATTERN = "/scsi2pi_sclp-XXXXXX";

    inline static const string FILE_PRINT_COUNT = "file_print_count";
    inline static const string BYTE_RECEIVE_COUNT = "byte_receive_count";
    inline static const string PRINT_ERROR_COUNT = "print_error_count";
    inline static const string PRINT_WARNING_COUNT = "print_warning_count";
};
