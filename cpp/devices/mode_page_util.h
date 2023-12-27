//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Shared code for SCSI command implementations
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <map>
#include "shared/scsi.h"

using namespace std;

namespace mode_page_util
{
string ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int, int);
void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
void AddAppleVendorModePage(map<int, vector<byte>>&, bool);
}
