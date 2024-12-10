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
    int WriteFilemark();
    int ReadWrite(span<uint8_t>, int);

private:

    int SpaceBack();
    void SetInt24(span<uint8_t>, int, int);

    unique_ptr<S2pDumpExecutor> s2pdump_executor;

    // TODO 0xffffff
    int default_length = 0x0fffff;

    static const int SHORT_TIMEOUT = 3;
    static const int LONG_TIMEOUT = 300;
};
