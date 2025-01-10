//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "shared/s2p_defs.h"

using namespace std;

class ScriptGenerator
{

public:

    bool CreateFile(const string&);

    void AddCdb(int, int, cdb_t);
    void AddData(span<const uint8_t>);

    void WriteEol();

private:

    ofstream file;
};
