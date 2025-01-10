//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"
#include <vector>
#include "shared/s2p_version.h"

int main(int argc, char *argv[])
{
    vector<char*> args(argv, argv + argc);

    return S2pSimh().Run(args);
}
