//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>
#include "shared/s2p_defs.h"

class PrimaryDevice;

class PageHandler final
{

public:

    PageHandler(PrimaryDevice&, bool, bool, bool);

    int AddModePages(int, int, int) const;

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    void ValidateCdb() const;

    void ModeSelect(int) const;

    PrimaryDevice &device;

    bool supports_mode_select;

    bool supports_save_parameters;

    bool supports_block_descriptors;
};
