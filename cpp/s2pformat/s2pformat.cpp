//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pformat_core.h"
#include <vector>

int main(int argc, char *argv[])
{
    vector<char*> args(argv, argv + argc);

    return S2pFormat().Run(args);
}
