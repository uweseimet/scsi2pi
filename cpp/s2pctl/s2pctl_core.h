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

    int Run(const vector<char*>&);

private:

    void Banner(bool) const;
    int RunInteractive();
    int ParseArguments(const vector<char*>&);

    // Preserve host settings during invocations in interactive mode
    string hostname = "localhost";
    int port = 6868;
};
