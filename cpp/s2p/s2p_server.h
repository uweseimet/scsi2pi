//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <string>

using namespace std;

class S2pServer final
{

public:

    string Init(int);

    void CleanUp();

    int Accept() const;

    bool IsRunning() const
    {
        return running;
    }

private:

    atomic<int> server_socket = -1;

    atomic<bool> running = false;
};
