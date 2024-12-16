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
#include <string>

using namespace std;

class S2pFormatter
{

public:

    string FormatBytes(span<const uint8_t>, size_t, bool = false) const;

    void SetLimit(int limit)
    {
        format_limit = limit;
    }

private:

    int format_limit = numeric_limits<int>::max();
};
