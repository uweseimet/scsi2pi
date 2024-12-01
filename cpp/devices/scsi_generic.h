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
    void WriteData(data_out_t, scsi_command, int) override;

private:

    int ReadWriteData(void*, bool);

    string device;

    int count = 0;

    int timeout = 0;

    int fd = -1;

    // The sense data returned by the SG driver, to be returned in the next REQUEST SENSE
    array<uint8_t, 18> deferred_sense_data = { };
    bool deferred_sense_data_valid = false;

    inline static const unordered_set<scsi_command> WRITE_COMMANDS = { scsi_command::write_6, scsi_command::write_10,
        scsi_command::write_16, scsi_command::verify_10, scsi_command::verify_16, scsi_command::write_long_10,
        scsi_command::write_long_16, scsi_command::mode_select_6, scsi_command::mode_select_10,
        scsi_command::execute_operation };

    // Linux limits the number of bytes that can be transferred in one go
    static const int MAX_TRANSFER_LENGTH = 65536;

    static constexpr const char *DEVICE = "device";
    static constexpr const char *TIMEOUT = "timeout";
};
