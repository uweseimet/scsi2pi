//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "phase_handler.h"

using namespace scsi_defs;

void PhaseHandler::Init()
{
    phase_executors[phase_t::busfree] = [this]() {BusFree();};
    phase_executors[phase_t::selection] = [this]() {Selection();};
    phase_executors[phase_t::dataout] = [this]() {DataOut();};
    phase_executors[phase_t::datain] = [this]() {DataIn();};
    phase_executors[phase_t::command] = [this]() {Command();};
    phase_executors[phase_t::status] = [this]() {Status();};
    phase_executors[phase_t::msgout] = [this]() {MsgOut();};
    phase_executors[phase_t::msgin] = [this]() {MsgIn();};
}
