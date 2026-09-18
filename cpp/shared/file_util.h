//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <string>

using namespace std;
using namespace filesystem;

namespace file_util
{

bool IsReadOnlyFile(const path&);

string GetExtensionLowerCase(const path&);

off_t GetCapacityFromFile(const path&);

}
