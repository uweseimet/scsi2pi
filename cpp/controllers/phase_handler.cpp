//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "phase_handler.h"
#include <cassert>

bool PhaseHandler::ProcessPhase()
{
    assert(phase <= BusPhase::RESERVED);

    switch (phase) {
    case BusPhase::BUS_FREE:
        BusFree();
        return true;

    case BusPhase::SELECTION:
        Selection();
        return true;

    case BusPhase::COMMAND:
        Command();
        return true;

    case BusPhase::DATA_IN:
        DataIn();
        return true;

    case BusPhase::DATA_OUT:
        DataOut();
        return true;

    case BusPhase::STATUS:
        Status();
        return true;

    case BusPhase::MSG_IN:
        MsgIn();
        return true;

    case BusPhase::MSG_OUT:
        MsgOut();
        return true;

    case BusPhase::ARBITRATION:
    case BusPhase::RESELECTION:
    case BusPhase::RESERVED:
    default:
        return false;
    }
}

