//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/property_handler.h"
#include <span>

namespace s2p_parser
{

void Banner(bool);
property_map ParseArguments(span<char*>, bool&);

}
