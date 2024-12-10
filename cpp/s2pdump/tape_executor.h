//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "s2pdump_executor.h"

using namespace std;

class TapeExecutor : public S2pDumpExecutor
{

public:

    TapeExecutor(Bus &b, int i, logger &l) : S2pDumpExecutor(b, i, l)
    {
    }

    int Rewind();
    int Space(bool, int);
    int ReadWrite(span<uint8_t>, bool);

private:

    unique_ptr<S2pDumpExecutor> s2pdump_executor;

    // TODO 0xffffff
    int default_length = 0x0fffff;
};
