//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

class S2pServer
{

public:

    string Init(int);

    void CleanUp();

    int Accept() const;

    bool IsRunning() const
    {
        return server_socket != -1;
    }

private:

    int server_socket = -1;
};
