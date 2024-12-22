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
#include "shared/scsi.h"

using namespace std;

class SgAdapter
{

public:

    using SgResult = struct _SgResult {
        _SgResult(int s, int l, sense_key k) : status(s), length(l), key(k) {}

        int status;
        int length;
        sense_key key;
    };

    string Init(const string&);
    void CleanUp();

    SgResult SendCommand(span<uint8_t>, span<uint8_t>, int, int) const;

private:

    int fd = -1;
};
