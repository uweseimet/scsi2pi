//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>

using namespace std;

enum class ShutdownMode
{
    NONE,
    STOP_S2P,
    STOP_PI,
    RESTART_PI
};

enum class ProtobufFormat
{
    BINARY = 0b001,
    JSON = 0b010,
    TEXT = 0b100
};

// Command Descriptor Block
using cdb_t = span<const int>;

using data_in_t = span<uint8_t>;
using data_out_t = span<const uint8_t>;

// A combination of device ID and LUN
using id_set = pair<int, int>;

// For work-around required by the DaynaPort emulation
static constexpr int SEND_NO_DELAY = -1;
