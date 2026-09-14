//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <string>

using namespace std;

namespace user_util
{

string GetAppDir();
int GetEuid();
pair<int, int> GetUidAndGid();

inline constexpr const char *DEFAULT_APP_FOLDER = "/var/lib/piscsi";

}
