//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <fstream>
#include <span>

using namespace std;

class ScriptGenerator
{

public:

    bool CreateFile(const string&);

    void AddCdb(int, int, span<int>);
    void AddData(span<uint8_t>);

    void WriteEol();

private:

    ofstream file;
};
