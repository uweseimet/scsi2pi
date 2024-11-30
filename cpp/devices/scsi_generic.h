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
    enum sense_key deferred_sense_key = sense_key::no_sense;
    enum asc deferred_asc = asc::no_additional_sense_information;
    uint8_t deferred_ascq = 0;

    inline static const unordered_set<scsi_command> WRITE_COMMANDS = { scsi_command::write_6, scsi_command::write_10,
        scsi_command::write_16, scsi_command::verify_10, scsi_command::verify_16, scsi_command::write_long_10,
        scsi_command::write_long_16, scsi_command::mode_select_6, scsi_command::mode_select_10,
        scsi_command::execute_operation };

    static constexpr const char *DEVICE = "device";
    static constexpr const char *TIMEOUT = "timeout";
};
