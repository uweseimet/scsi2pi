//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <string>

using namespace std;

namespace sg_util
{

int OpenDevice(const string&);

int GetAllocationLength(span<uint8_t>);

void UpdateStartBlock(span<uint8_t>, int);
void SetBlockCount(span<uint8_t>, int);

void SetInt24(span<uint8_t>, int, int);

};
