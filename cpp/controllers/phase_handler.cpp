//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "phase_handler.h"

void PhaseHandler::Init()
{
    phase_executors[static_cast<int>(bus_phase::busfree)] = [this]() {BusFree();};
    phase_executors[static_cast<int>(bus_phase::arbitration)] = []() {throw invalid_argument("");};
    phase_executors[static_cast<int>(bus_phase::selection)] = [this]() {Selection();};
    phase_executors[static_cast<int>(bus_phase::reselection)] = []() {throw invalid_argument("");};
    phase_executors[static_cast<int>(bus_phase::command)] = [this]() {Command();};
    phase_executors[static_cast<int>(bus_phase::datain)] = [this]() {DataIn();};
    phase_executors[static_cast<int>(bus_phase::dataout)] = [this]() {DataOut();};
    phase_executors[static_cast<int>(bus_phase::status)] = [this]() {Status();};
    phase_executors[static_cast<int>(bus_phase::msgin)] = [this]() {MsgIn();};
    phase_executors[static_cast<int>(bus_phase::msgout)] = [this]() {MsgOut();};
    phase_executors[static_cast<int>(bus_phase::reserved)] = []() {throw invalid_argument("");};
}
