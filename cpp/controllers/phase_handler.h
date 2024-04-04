//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <functional>
#include "shared/scsi.h"

using namespace std;

class PhaseHandler
{

public:

    PhaseHandler() = default;
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

    inline bus_phase GetPhase() const
    {
        return phase;
    }
    inline void SetPhase(bus_phase p)
    {
        phase = p;
    }
    inline bool IsSelection() const
    {
        return phase == bus_phase::selection;
    }
    inline bool IsBusFree() const
    {
        return phase == bus_phase::busfree;
    }
    inline bool IsCommand() const
    {
        return phase == bus_phase::command;
    }
    inline bool IsStatus() const
    {
        return phase == bus_phase::status;
    }
    inline bool IsDataIn() const
    {
        return phase == bus_phase::datain;
    }
    inline bool IsDataOut() const
    {
        return phase == bus_phase::dataout;
    }
    inline bool IsMsgIn() const
    {
        return phase == bus_phase::msgin;
    }
    inline bool IsMsgOut() const
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
