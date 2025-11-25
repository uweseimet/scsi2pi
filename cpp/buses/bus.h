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

// Constant declarations (Control signals)
constexpr static int TAD_OUT = !TAD_IN;
constexpr static int IND_OUT = !IND_IN;
constexpr static int DTD_OUT = !DTD_IN;

using namespace std;

class Bus // NOSONAR The high number of convenience methods is justified
{

public:

    virtual ~Bus() = default;

    virtual bool Init(bool);
    virtual void CleanUp() = 0;

    virtual void Acquire() = 0;

    virtual bool WaitForSelection() = 0;

    virtual void SetBSY(bool) = 0;

    virtual void SetSEL(bool) = 0;

    virtual uint8_t GetDAT();
    virtual void SetDAT(uint8_t) = 0;

    virtual bool GetControl(int) const;
    virtual void SetControl(int, bool) = 0;

    virtual bool IsRaspberryPi() const = 0;

    virtual void SetDir(bool) = 0;

    virtual bool WaitHandshake(int, bool);

    int CommandHandShake(span<uint8_t>);
    int InitiatorMsgInHandShake();
    int TargetReceiveHandShake(uint8_t*, int);
    int InitiatorReceiveHandShake(uint8_t*, int);
    int TargetSendHandShake(const uint8_t*, int, int = SEND_NO_DELAY);
    int InitiatorSendHandShake(const uint8_t*, int);

    virtual void Reset();

    uint32_t GetSignals() const
    {
        return signals;
    }
    void SetSignals(uint32_t s)
    {
        signals = s;
    }

    bool GetBSY() const
    {
        return GetControl(PIN_BSY_MASK);
    }

    bool GetSEL() const
    {
        return GetControl(PIN_SEL_MASK);
    }

    bool GetREQ() const
    {
        return GetControl(PIN_REQ_MASK);
    }
    void SetREQ(bool state)
    {
        SetControl(PIN_REQ, state);
    }

    bool GetATN() const
    {
        return GetControl(PIN_ATN_MASK);
    }
    void SetATN(bool state)
    {
        SetControl(PIN_ATN, state);
    }

    bool GetACK() const
    {
        return GetControl(PIN_ACK_MASK);
    }
    void SetACK(bool state)
    {
        SetControl(PIN_ACK, state);
    }

    bool GetRST() const
    {
        return GetControl(PIN_RST_MASK);
    }
    void SetRST(bool state)
    {
        SetControl(PIN_RST, state);
    }

    bool GetMSG() const
    {
        return GetControl(PIN_MSG_MASK);
    }
    void SetMSG(bool state)
    {
        SetControl(PIN_MSG, state);
    }

    bool GetCD() const
    {
        return GetControl(PIN_CD_MASK);
    }
    void SetCD(bool state)
    {
        SetControl(PIN_CD, state);
    }

    bool GetIO() const
    {
        return GetControl(PIN_IO_MASK);
    }
    void SetIO(bool);

    BusPhase GetPhase();

    static string GetPhaseName(BusPhase phase)
    {
        return phase_names[static_cast<int>(phase)];
    }

protected:

    Bus() = default;

    virtual void WaitBusSettle() const = 0;

    virtual void EnableIRQ() = 0;
    virtual void DisableIRQ() = 0;

    bool IsTarget() const
    {
        return target_mode;
    }

private:

    int HandshakeTimeoutError();

    bool IsPhase(BusPhase phase)
    {
        return GetPhase() == phase;
    }

    bool target_mode = true;

    // All bus signals
    uint32_t signals = 0;

    static const array<BusPhase, 8> phases;

    static const array<string, 11> phase_names;

    // The DaynaPort SCSI Link do a short delay in the middle of transfering
    // a packet. This is the number of ns that will be delayed between the
    // header and the actual data.
    static constexpr int DAYNAPORT_SEND_DELAY_NS = 100'000;
};
