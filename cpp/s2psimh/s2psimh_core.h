//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <limits>
#include <span>
#include <string>
#include "shared/simh_util.h"

using namespace std;

class S2pSimh
{

public:

    int Run(span<char*>);

private:

    static void Banner(bool);

    bool ParseArguments(span<char*>);

    int Analyze();

    void PrintClass(const simh_util::SimhMetaData&) const;
    void PrintValue(const simh_util::SimhMetaData&);
    bool PrintRecord(const string&, const simh_util::SimhMetaData&);
    bool PrintReservedMarker(const simh_util::SimhMetaData&);

    bool ReadRecord(span<uint8_t>);

    string filename;

    off_t file_size;

    fstream file;

    int64_t position = 0;
    int64_t old_position = 0;

    bool dump = false;
    uint32_t limit = numeric_limits<uint32_t>::max();
};
