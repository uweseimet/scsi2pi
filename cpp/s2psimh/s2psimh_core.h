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

    int Analyze(istream&, off_t);

    void PrintClass(simh_util::simh_class) const;
    void PrintValue(int);
    bool PrintRecord(const string&, int);
    bool PrintReservedMarker(int);

    string filename;

    ifstream file;

    int64_t position = 0;
    int64_t old_position = 0;

    bool dump = false;
    int limit = numeric_limits<int>::max();
};
