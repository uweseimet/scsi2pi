//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <string>
#include "shared/scsi.h"
#include "shared/s2p_defs.h"
#include "board.h"

class Bus // NOSONAR The high number of simple convenience methods is justified
{

public:

    virtual ~Bus() = default;

    bool Init(bool);
    virtual void Reset() const;

    virtual bool SetUp(bool)
    {
        // Nothing to do
        return true;
    }

    virtual void CleanUp()
    {
        // Nothing to do
    }

    virtual void Acquire() const = 0;

    virtual uint8_t WaitForSelection() = 0;

    virtual void SetDAT(uint8_t) const = 0;

    virtual bool GetSignal(int) const;

    virtual bool IsRaspberryPi() const = 0;

    virtual void SetDir(bool) const = 0;

    virtual bool WaitHandShake(int, bool) const;

    int TargetCommandHandShake(data_in_t);
    int TargetReceiveHandShake(data_in_t);
    int TargetSendHandShake(data_out_t, int = SEND_NO_DELAY);
    int InitiatorMsgInHandShake() const;
    int InitiatorReceiveHandShake(data_in_t);
    int InitiatorSendHandShake(data_out_t);

    uint8_t GetDAT() const
    {
        // A bus settle delay
        WaitNanoSeconds(false);

        Acquire();

        return static_cast<uint8_t>(~(signals >> PIN_DT0));
    }

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
    virtual void SetBSY(bool state) const;

    bool GetSEL() const
    {
        return GetSignal(PIN_SEL_MASK);
    }
    virtual void SetSEL(bool state) const
    {
        SetSignal(PIN_SEL, state);
    }

    bool GetREQ() const
    {
        return GetSignal(PIN_REQ_MASK);
    }
    void SetREQ(bool state) const
    {
        SetSignal(PIN_REQ, state);
    }

    bool GetATN() const
    {
        return GetSignal(PIN_ATN_MASK);
    }
    void SetATN(bool state) const
    {
        SetSignal(PIN_ATN, state);
    }

    bool GetACK() const
    {
        return GetSignal(PIN_ACK_MASK);
    }
    void SetACK(bool state) const
    {
        SetSignal(PIN_ACK, state);
    }

    bool GetRST() const
    {
        return GetSignal(PIN_RST_MASK);
    }
    void SetRST(bool state) const
    {
        SetSignal(PIN_RST, state);
    }

    bool GetMSG() const
    {
        return GetSignal(PIN_MSG_MASK);
    }
    void SetMSG(bool state) const
    {
        SetSignal(PIN_MSG, state);
    }

    bool GetCD() const
    {
        return GetSignal(PIN_CD_MASK);
    }
    void SetCD(bool state) const
    {
        SetSignal(PIN_CD, state);
    }

    bool GetIO() const
    {
        return GetSignal(PIN_IO_MASK);
    }
    void SetIO(bool) const;

    virtual BusPhase GetPhase() const
    {
        Acquire();

        // Get phase from bus signal lines SEL, BSY, I/O, C/D and MSG
        return phases[(signals >> PIN_MSG) & 0b11111];
    }

    virtual bool IsPhase(BusPhase phase) const
    {
        // The signals are still up to date
        return phases[(signals >> PIN_MSG) & 0b11111] == phase;
    }

    static string GetPhaseName(BusPhase phase)
    {
        return phase_names[static_cast<int>(phase)];
    }

protected:

    Bus() = default;

    virtual void SetSignal(int, bool) const = 0;

    virtual void WaitNanoSeconds(bool) const = 0;

    virtual void EnableIRQ() = 0;
    virtual void DisableIRQ() = 0;

    uint8_t GetSelection() const;

    // The DaynaPort SCSI Link do a short delay in the middle of transfering
    // a packet. This is the number of ns that will be delayed between the
    // header and the actual data.
    constexpr static int DAYNAPORT_SEND_DELAY_NS = 100'000;

private:

    int CommandHandshakeTimeout();

    // The current bus signals, static because there is exactly one set of bus signals
    inline static uint32_t signals = 0xffffffff;

    static const array<BusPhase, 32> phases;

    static const array<string, 11> phase_names;
};
