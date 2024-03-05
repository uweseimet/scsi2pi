//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <functional>
#include "shared/scsi.h"

using namespace scsi_defs;

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
    virtual void MsgOut()
    {
        // To be implemented by controllers supporting this phase (SCSI, but not SASI)
    }

protected:

    inline phase_t GetPhase() const
    {
        return phase;
    }
    inline void SetPhase(phase_t p)
    {
        phase = p;
    }
    inline bool IsSelection() const
    {
        return phase == phase_t::selection;
    }
    inline bool IsBusFree() const
    {
        return phase == phase_t::busfree;
    }
    inline bool IsCommand() const
    {
        return phase == phase_t::command;
    }
    inline bool IsStatus() const
    {
        return phase == phase_t::status;
    }
    inline bool IsDataIn() const
    {
        return phase == phase_t::datain;
    }
    inline bool IsDataOut() const
    {
        return phase == phase_t::dataout;
    }
    inline bool IsMsgIn() const
    {
        return phase == phase_t::msgin;
    }
    inline bool IsMsgOut() const
    {
        return phase == phase_t::msgout;
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

    phase_t phase = phase_t::busfree;

    array<function<void()>, 11> phase_executors;
};
