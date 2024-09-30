//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <vector>
#include "base/primary_device.h"

class PageHandler
{

public:

    PageHandler(PrimaryDevice&, bool, bool);

    int AddModePages(cdb_t, vector<uint8_t>&, int, int, int) const;

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    void ModeSelect(int) const;

    PrimaryDevice &device;

    bool supports_mode_select;

    bool supports_save_parameters;
};
