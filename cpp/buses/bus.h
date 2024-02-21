//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <array>
#include <vector>
#include "pin_control.h"
#include "shared/scsi.h"

using namespace std;
using namespace scsi_defs;

class Bus : public PinControl
{

public:

    virtual bool Init(bool) = 0;
    virtual void Reset() = 0;
    virtual void CleanUp() = 0;

    phase_t GetPhase();
    static phase_t GetPhase(int mci)
    {
        return phases[mci];
    }
    static string GetPhaseName(phase_t);

    virtual uint32_t Acquire() = 0;
    virtual int CommandHandShake(vector<uint8_t>&) = 0;
    virtual int MsgInHandShake() = 0;
    virtual int ReceiveHandShake(uint8_t*, int) = 0;
    virtual int SendHandShake(uint8_t*, int, int = SEND_NO_DELAY) = 0;

    virtual bool WaitREQ(bool) = 0;
    virtual bool WaitACK(bool) = 0;

    virtual bool WaitForSelection() = 0;

    virtual bool GetSignal(int) const = 0;
    virtual void SetSignal(int, bool) = 0;

    // Work-around needed for the DaynaPort emulation
    static const int SEND_NO_DELAY = -1;

private:

    static const array<phase_t, 8> phases;

    static const unordered_map<phase_t, string> phase_names;
};
