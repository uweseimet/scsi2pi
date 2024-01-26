//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include "shared/scsi.h"

using namespace std;

namespace mode_page_util
{
int ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int, int);
int EvaluateBlockDescriptors(scsi_defs::scsi_command, span<const uint8_t>, int, int&);
int HandleSectorSizeChange(span<const uint8_t>, int, int, bool);
void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
void AddAppleVendorModePage(map<int, vector<byte>>&, bool);
}
