//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <string>

using namespace std;

class S2pSimh
{

public:

    int Run(span<char*>);

private:

    static void Banner(bool);

    int Analyze(const string&);
};
