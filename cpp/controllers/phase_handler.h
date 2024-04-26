//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <stdexcept>
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

    bus_phase GetPhase() const
    {
        return phase;
    }
    void SetPhase(bus_phase p)
    {
        phase = p;
    }
    bool IsSelection() const
    {
        return phase == bus_phase::selection;
    }
    bool IsBusFree() const
    {
        return phase == bus_phase::busfree;
    }
    bool IsCommand() const
    {
        return phase == bus_phase::command;
    }
    bool IsStatus() const
    {
        return phase == bus_phase::status;
    }
    bool IsDataIn() const
    {
        return phase == bus_phase::datain;
    }
    bool IsDataOut() const
    {
        return phase == bus_phase::dataout;
    }
    bool IsMsgIn() const
    {
        return phase == bus_phase::msgin;
    }
    bool IsMsgOut() const
    {
        return phase == bus_phase::msgout;
    }

    bool ProcessPhase() const
    {
        try {
            phase_executors[static_cast<int>(phase)]();
        }
        catch (const invalid_argument&) {
            return false;
        }

        return true;
    }

private:

    bus_phase phase = bus_phase::busfree;

    array<function<void()>, 11> phase_executors;
};
