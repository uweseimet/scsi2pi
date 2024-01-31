//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pproto_core.h"

int main(int argc, char *argv[])
{
    vector<char*> args(argv, argv + argc);

    return S2pProto().Run(args, false);
}
