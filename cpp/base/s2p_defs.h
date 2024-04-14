//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>

using namespace std;

enum class shutdown_mode
{
    none,
    stop_s2p,
    stop_pi,
    restart_pi
};

// Command Descriptor Block
using cdb_t = span<const int>;

// A combination of device ID and LUN
using id_set = pair<int, int>;

// For work-around required by the DaynaPort emulation
static constexpr int SEND_NO_DELAY = -1;
