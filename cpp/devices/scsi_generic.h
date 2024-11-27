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

    int ReadData(span<uint8_t>) override;
    void WriteData(span<const uint8_t>, scsi_command, int) override;

private:

    int ReadWriteData(span<uint8_t>, bool) const;

    string device;

    int count = 0;

    int timeout = 0;

    int fd = -1;

    static constexpr const char *DEVICE = "device";
    static constexpr const char *TIMEOUT = "timeout";
};
