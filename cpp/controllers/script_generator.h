//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "shared/s2p_defs.h"

class ScriptGenerator final
{

public:

    bool CreateFile(const string&);

    void AddCdb(int, int, cdb_t);
    void AddData(span<const uint8_t>);

private:

    ofstream file;
};
