//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2psimh_core.h"

int main(int argc, char *argv[])
{
    return S2pSimh().Run( { argv, static_cast<size_t>(argc) });
}
