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
    phase_executors[static_cast<int>(BusPhase::BUS_FREE)] = [this]() {BusFree(); return true;};
    phase_executors[static_cast<int>(BusPhase::ARBITRATION)] = []() {return false;};
    phase_executors[static_cast<int>(BusPhase::SELECTION)] = [this]() {Selection(); return true;};
    phase_executors[static_cast<int>(BusPhase::RESELECTION)] = []() {return false;};
    phase_executors[static_cast<int>(BusPhase::COMMAND)] = [this]() {Command(); return true;};
    phase_executors[static_cast<int>(BusPhase::DATA_IN)] = [this]() {DataIn(); return true;};
    phase_executors[static_cast<int>(BusPhase::DATA_OUT)] = [this]() {DataOut(); return true;};
    phase_executors[static_cast<int>(BusPhase::STATUS)] = [this]() {Status(); return true;};
    phase_executors[static_cast<int>(BusPhase::MSG_IN)] = [this]() {MsgIn(); return true;};
    phase_executors[static_cast<int>(BusPhase::MSG_OUT)] = [this]() {MsgOut(); return true;};
    phase_executors[static_cast<int>(BusPhase::RESERVED)] = []() {return false;};
}
