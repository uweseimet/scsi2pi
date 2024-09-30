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
#include "base/property_handler.h"
#include "base/primary_device.h"

class PageHandler
{

public:

    PageHandler(PrimaryDevice&, bool, bool);

    int AddModePages(cdb_t, vector<uint8_t>&, int, int, int) const;

private:

    void SaveParametersCheck(int) const;

    PrimaryDevice &device;

    bool supports_mode_select;

    bool supports_save_parameters;

    PropertyHandler &property_handler = PropertyHandler::Instance();
};
