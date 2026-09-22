//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

using namespace std;

class S2pFormatter final
{

public:

    string FormatBytes(span<const uint8_t>, size_t, bool = false) const;

    bool SetLimit(int);

private:

    int format_limit = numeric_limits<int>::max();
};
