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

    TapeExecutor(Bus &bus, int id, logger &l) : S2pDumpExecutor(bus, id, l)
    {
    }

    int Rewind();
    int WriteFilemark();
    int ReadWrite(span<uint8_t>, int);

    static const int NO_MORE_DATA = -1;
    static const int BAD_BLOCK = -2;

private:

    void SpaceBack();

    static void SetInt24(span<uint8_t>, int, int);

    unique_ptr<S2pDumpExecutor> s2pdump_executor;

    // TODO Must be 0xffffff, requires TODO in tape.cpp to be fixed
    int default_length = 0x000200;

    static const int SHORT_TIMEOUT = 3;
    static const int LONG_TIMEOUT = 300;
};
