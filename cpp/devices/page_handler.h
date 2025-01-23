//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <vector>
#include "shared/s2p_defs.h"

class PrimaryDevice;

class PageHandler
{

public:

    PageHandler(PrimaryDevice&, bool, bool);

    int AddModePages(cdb_t, data_in_t, int, int, int) const;

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    void ModeSelect(int) const;

    PrimaryDevice &device;

    bool supports_mode_select;

    bool supports_save_parameters;
};
