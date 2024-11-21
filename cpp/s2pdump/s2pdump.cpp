//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pdump_core.h"

int main(int argc, char *argv[])
{
    vector<char*> args(argv, argv + argc);

    return S2pDump().Run(args, false);
}
