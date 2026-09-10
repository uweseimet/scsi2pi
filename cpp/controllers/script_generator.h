//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <string>
#include "shared/s2p_defs.h"

class ScriptGenerator final
{

public:

    bool CreateFile(const string&);

    bool AddCdb(int, int, cdb_t);
    bool AddData(span<const uint8_t>);

private:

    ofstream file;
};
