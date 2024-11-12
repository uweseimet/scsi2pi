//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <fstream>

using namespace std;

namespace simh_util
{

pair<int, int> ReadHeader(istream&, int64_t&);
int WriteHeader(ostream&, int64_t, int, uint32_t, uint32_t);

int64_t MoveBack(istream&, int64_t);

static const int64_t HEADER_SIZE = static_cast<int64_t>(sizeof(uint32_t));

static const int OVERFLOW_ERROR = -1;
static const int WRITE_ERROR = -2;

}
