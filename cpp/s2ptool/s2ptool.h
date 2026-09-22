//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

using namespace std;

namespace s2ptool
{

void Usage();

void AddArg(vector<char*>&, const string&);

}
