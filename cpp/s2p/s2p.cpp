//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_core.h"

int main(int argc, char *argv[])
{
    return S2p().Run( { argv, static_cast<size_t>(argc) });
}
