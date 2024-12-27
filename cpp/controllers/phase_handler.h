//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <functional>
#include "shared/scsi.h"

using namespace std;

class PhaseHandler
{

public:

    virtual ~PhaseHandler() = default;

    void Init();

    virtual void BusFree() = 0;
    virtual void Selection() = 0;
    virtual void Command() = 0;
    virtual void Status() = 0;
    virtual void DataIn() = 0;
    virtual void DataOut() = 0;
    virtual void MsgIn() = 0;
    virtual void MsgOut() = 0;

protected:

    PhaseHandler() = default;

    BusPhase GetPhase() const
    {
        return phase;
    }
    void SetPhase(BusPhase p)
    {
        phase = p;
    }
    bool IsSelection() const
    {
        return phase == BusPhase::SELECTION;
    }
    bool IsBusFree() const
    {
        return phase == BusPhase::BUS_FREE;
    }
    bool IsCommand() const
    {
        return phase == BusPhase::COMMAND;
    }
    bool IsStatus() const
    {
        return phase == BusPhase::STATUS;
    }
    bool IsDataIn() const
    {
        return phase == BusPhase::DATA_IN;
    }
    bool IsDataOut() const
    {
        return phase == BusPhase::DATA_OUT;
    }
    bool IsMsgIn() const
    {
        return phase == BusPhase::MSG_IN;
    }
    bool IsMsgOut() const
    {
        return phase == BusPhase::MSG_OUT;
    }

    bool ProcessPhase() const
    {
        assert(phase <= BusPhase::RESERVED);
        return phase_executors[static_cast<int>(phase)]();
    }

private:

    BusPhase phase = BusPhase::BUS_FREE;

    array<function<bool()>, static_cast<int>(BusPhase::RESERVED) + 1> phase_executors;
};
