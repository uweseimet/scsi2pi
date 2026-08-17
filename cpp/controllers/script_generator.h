//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "shared/s2p_defs.h"

class ScriptGenerator final
{

public:

    bool CreateFile(string_view);

    void AddCdb(int, int, cdb_t);
    void AddData(span<const uint8_t>);

private:

    ofstream file;
};
