//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "phase_handler.h"

void PhaseHandler::Init()
{
    phase_executors[static_cast<int>(bus_phase::busfree)] = [this]() {BusFree(); return true;};
    phase_executors[static_cast<int>(bus_phase::arbitration)] = []() {return false;};
    phase_executors[static_cast<int>(bus_phase::selection)] = [this]() {Selection(); return true;};
    phase_executors[static_cast<int>(bus_phase::reselection)] = []() {return false;};
    phase_executors[static_cast<int>(bus_phase::command)] = [this]() {Command(); return true;};
    phase_executors[static_cast<int>(bus_phase::datain)] = [this]() {DataIn(); return true;};
    phase_executors[static_cast<int>(bus_phase::dataout)] = [this]() {DataOut(); return true;};
    phase_executors[static_cast<int>(bus_phase::status)] = [this]() {Status(); return true;};
    phase_executors[static_cast<int>(bus_phase::msgin)] = [this]() {MsgIn(); return true;};
    phase_executors[static_cast<int>(bus_phase::msgout)] = [this]() {MsgOut(); return true;};
    phase_executors[static_cast<int>(bus_phase::reserved)] = []() {return false;};
}
