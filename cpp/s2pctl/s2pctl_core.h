//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <string>

using namespace std;

class S2pCtl
{

public:

    int Run(const vector<char*>&) const;

private:

    void Banner(bool) const;
    int RunInteractive() const;
    int ParseArguments(const vector<char*>&) const;

    static string ConvertCommand(const string&);
};
