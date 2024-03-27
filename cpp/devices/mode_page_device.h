//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/property_handler.h"
#include "base/primary_device.h"

class ModePageDevice : public PrimaryDevice
{
public:

    ModePageDevice(PbDeviceType type, scsi_level level, int lun, bool m, bool s)
    : PrimaryDevice(type, level, lun), supports_mode_select(m), supports_save_parameters(s)
    {
    }
    ~ModePageDevice() override = default;

    bool Init(const param_map&) override;

    virtual void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int);

protected:

    int AddModePages(cdb_t, vector<uint8_t>&, int, int, int) const;
    virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const = 0;
    virtual void AddVendorPages(map<int, vector<byte>>&, int, bool) const
    {
        // Nothing to add by default
    }

private:

    virtual int ModeSense6(cdb_t, vector<uint8_t>&) const = 0;
    virtual int ModeSense10(cdb_t, vector<uint8_t>&) const = 0;

    void ModeSense6() const;
    void ModeSense10() const;
    void ModeSelect6() const;
    void ModeSelect10() const;

    void SaveParametersCheck(int) const;

    bool supports_mode_select;

    bool supports_save_parameters;

    PropertyHandler &property_handler = PropertyHandler::Instance();
};
