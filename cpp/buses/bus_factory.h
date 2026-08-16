//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include "bus.h"

namespace bus_factory
{

unique_ptr<Bus> CreateBus(bool, bool, bool, const string&);

}
