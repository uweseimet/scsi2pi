//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "phase_handler.h"

using namespace scsi_defs;

void PhaseHandler::Init()
{
    phase_executors[static_cast<int>(phase_t::busfree)] = [this]() {BusFree();};
    phase_executors[static_cast<int>(phase_t::arbitration)] = []() {throw invalid_argument("");};
    phase_executors[static_cast<int>(phase_t::selection)] = [this]() {Selection();};
    phase_executors[static_cast<int>(phase_t::reselection)] = []() {throw invalid_argument("");};
    phase_executors[static_cast<int>(phase_t::command)] = [this]() {Command();};
    phase_executors[static_cast<int>(phase_t::datain)] = [this]() {DataIn();};
    phase_executors[static_cast<int>(phase_t::dataout)] = [this]() {DataOut();};
    phase_executors[static_cast<int>(phase_t::status)] = [this]() {Status();};
    phase_executors[static_cast<int>(phase_t::msgin)] = [this]() {MsgIn();};
    phase_executors[static_cast<int>(phase_t::msgout)] = [this]() {MsgOut();};
    phase_executors[static_cast<int>(phase_t::reserved)] = []() {throw invalid_argument("");};
}
