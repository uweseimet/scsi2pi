//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Implementation of a SCSI printer (see SCSI-2 specification for a command description)
//
//---------------------------------------------------------------------------
#pragma once

#include "base/primary_device.h"

using namespace std;

class ScsiGeneric : public PrimaryDevice
{

public:

    explicit ScsiGeneric(int);
    ~ScsiGeneric() override = default;

    bool SetUp() override;
    void CleanUp() override;

    void Dispatch(scsi_command) override;

    param_map GetDefaultParams() const override;

    vector<uint8_t> InquiryInternal() const override;

    int ReadData(data_in_t) override;
    int WriteData(cdb_t, data_out_t, int, int) override;

private:

    int ReadWriteData(span<uint8_t>, bool, int);

    int GetAllocationLength() const;
    void UpdateStartBlock(int);
    void SetBlockCount(int);

    void UpdateInternalBlockSize(span<uint8_t> buf, int);

    static void SetInt24(span<uint8_t>, int, int);

    string device;

    // The block size is update when a READ CAPACITY command is detected
    uint32_t block_size = 512;

    int count = 0;

    int byte_count = 0;
    int remaining_count = 0;

    int fd = -1;

    vector<uint8_t> local_cdb;

    vector<uint8_t> format_header;

    // The sense data returned by the SG driver, to be returned in the next REQUEST SENSE
    array<uint8_t, 18> deferred_sense_data = { };
    bool deferred_sense_data_valid = false;

    // Linux limits the number of bytes that can be transferred in a single SCSI request
    static const int MAX_TRANSFER_LENGTH = 65536;

    static const int TIMEOUT_DEFAULT_SECONDS = 5;

    // Sufficient for formatting a floppy disk in a USB floppy drive
    static const int TIMEOUT_FORMAT_SECONDS = 120;

    static constexpr const char *DEVICE = "device";
};
