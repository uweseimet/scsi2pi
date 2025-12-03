//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <string>
#include "shared/scsi.h"
#include "shared/s2p_defs.h"
#include "board.h"

// Constant declarations (SCSI)
constexpr static int IN = 0;
constexpr static int OUT = 1;

using namespace std;

class Bus // NOSONAR The high number of convenience methods is justified
{

public:

    virtual ~Bus() = default;

    virtual bool Init(bool);
    virtual void Reset();
    virtual void CleanUp() = 0;

    virtual void Acquire() const = 0;

    virtual uint8_t WaitForSelection() = 0;

    virtual void SetBSY(bool) = 0;

    virtual void SetSEL(bool) = 0;

    virtual bool GetIO() = 0;
    virtual void SetIO(bool) = 0;

    virtual uint8_t GetDAT() const;
    virtual void SetDAT(uint8_t) = 0;

    virtual bool GetSignal(int) const;
    virtual void SetSignal(int, bool) = 0;

    virtual bool IsRaspberryPi() const = 0;

    virtual bool WaitHandshake(int, bool) const;

    int CommandHandShake(data_in_t);
    int InitiatorMsgInHandShake();
    int TargetReceiveHandShake(data_in_t);
    int InitiatorReceiveHandShake(data_in_t);
    int TargetSendHandShake(data_out_t, int = SEND_NO_DELAY);
    int InitiatorSendHandShake(data_out_t);

    uint32_t GetSignals() const
    {
        return signals;
    }
    void SetSignals(uint32_t s) const
    {
        signals = s;
    }

    bool GetBSY() const
    {
        return GetSignal(PIN_BSY_MASK);
    }

    bool GetSEL() const
    {
        return GetSignal(PIN_SEL_MASK);
    }

    bool GetREQ() const
    {
        return GetSignal(PIN_REQ_MASK);
    }
    void SetREQ(bool state)
    {
        SetSignal(PIN_REQ, state);
    }

    bool GetATN() const
    {
        return GetSignal(PIN_ATN_MASK);
    }
    void SetATN(bool state)
    {
        SetSignal(PIN_ATN, state);
    }

    bool GetACK() const
    {
        return GetSignal(PIN_ACK_MASK);
    }
    void SetACK(bool state)
    {
        SetSignal(PIN_ACK, state);
    }

    bool GetRST() const
    {
        return GetSignal(PIN_RST_MASK);
    }
    void SetRST(bool state)
    {
        SetSignal(PIN_RST, state);
    }

    bool GetMSG() const
    {
        return GetSignal(PIN_MSG_MASK);
    }
    void SetMSG(bool state)
    {
        SetSignal(PIN_MSG, state);
    }

    bool GetCD() const
    {
        return GetSignal(PIN_CD_MASK);
    }
    void SetCD(bool state)
    {
        SetSignal(PIN_CD, state);
    }

    BusPhase GetPhase() const
    {
        Acquire();

        // Get phase from bus signal lines SEL, BSY, I/O, C/D and MSG
        return phases[(signals >> PIN_MSG) & 0b11111];
    }

    static string GetPhaseName(BusPhase phase)
    {
        return phase_names[static_cast<int>(phase)];
    }

protected:

    Bus() = default;

    virtual void WaitNanoSeconds(bool) const = 0;

    virtual void EnableIRQ() = 0;
    virtual void DisableIRQ() = 0;

    bool WaitForNotBusy() const;

    bool IsTarget() const
    {
        return target_mode;
    }

    // The DaynaPort SCSI Link do a short delay in the middle of transfering
    // a packet. This is the number of ns that will be delayed between the
    // header and the actual data.
    constexpr static int DAYNAPORT_SEND_DELAY_NS = 100'000;

private:

    int CommandHandshakeTimeout();

    bool IsPhase(BusPhase phase) const
    {
        // The signals are still up to date
        return phases[(signals >> PIN_MSG) & 0b11111] == phase;
    }

    bool target_mode = true;

    // All bus signals, mutable because they represent external state
    mutable uint32_t signals = 0xffffffff;

    static const array<BusPhase, 32> phases;

    static const array<string, 11> phase_names;
};
