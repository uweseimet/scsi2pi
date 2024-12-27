//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "shared/scsi.h"
#include "base/s2p_defs.h"
#include "board.h"

//---------------------------------------------------------------------------
//
// Control signal pin assignment setting
//  GPIO pin mapping table for control signals.
//
//  Control signal:
//   PIN_ACT
//     Signal that indicates the status of processing SCSI command.
//   PIN_ENB
//     Signal that indicates the valid signal from start to finish.
//   PIN_TAD
//     Signal that indicates the input/output direction of the target signal (BSY,IO,CD,MSG,REG).
//   PIN_IND
//     Signal that indicates the input/output direction of the initiator signal (SEL, ATN, RST, ACK).
//   PIN_DTD
//     Signal that indicates the input/output direction of the data lines (DT0...DT7,DP).
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// Control signal output logic
//   0V:FALSE  3.3V:TRUE
//
//   ACT_ON
//     PIN_ACT signal
//   ENB_ON
//     PIN_ENB signal
//   TAD_IN
//     PIN_TAD This is the logic when inputting.
//   IND_IN
//     PIN_ENB This is the logic when inputting.
//    DTD_IN
//     PIN_ENB This is the logic when inputting.
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// SCSI signal pin assignment setting
//   GPIO pin mapping table for SCSI signals.
//   PIN_DT0～PIN_SEL
//
//---------------------------------------------------------------------------

// Constant declarations (GPIO)
constexpr static int GPIO_INPUT = 0;
constexpr static int GPIO_OUTPUT = 1;
constexpr static int GPIO_PULLNONE = 0;
constexpr static int GPIO_PULLDOWN = 1;

// Constant declarations (Control signals)
#define ACT_OFF !ACT_ON
#define ENB_OFF !ENB_ON
#define TAD_OUT !TAD_IN
#define IND_OUT !IND_IN
#define DTD_OUT !DTD_IN

// Constant declarations (SCSI)
constexpr static int IN = GPIO_INPUT;
constexpr static int OUT = GPIO_OUTPUT;
constexpr static int ON = 1;
constexpr static int OFF = 0;

using namespace std;

class Bus // NOSONAR The number of convenience methods is justified
{

public:

    virtual ~Bus() = default;

    virtual bool Init(bool = true);
    virtual void Reset() = 0;
    virtual void CleanUp() = 0;

    virtual uint32_t Acquire() = 0;

    virtual bool WaitForSelection() = 0;

    virtual void SetBSY(bool) = 0;

    virtual void SetSEL(bool) = 0;

    virtual bool GetIO() = 0;
    virtual void SetIO(bool) = 0;

    virtual uint8_t GetDAT() = 0;
    virtual void SetDAT(uint8_t) = 0;

    virtual bool GetSignal(int) const = 0;
    virtual void SetSignal(int, bool) = 0;

    virtual bool IsRaspberryPi() const = 0;

    virtual bool WaitSignal(int, bool);

    int CommandHandShake(vector<uint8_t>&);
    int MsgInHandShake();
    int ReceiveHandShake(uint8_t*, int);
    int SendHandShake(const uint8_t*, int, int = SEND_NO_DELAY);

    bool GetBSY() const
    {
        return GetSignal(PIN_BSY);
    }

    bool GetSEL() const
    {
        return GetSignal(PIN_SEL);
    }

    bool GetREQ() const
    {
        return GetSignal(PIN_REQ);
    }

    void SetREQ(bool state)
    {
        SetSignal(PIN_REQ, state);
    }

    bool GetATN() const
    {
        return GetSignal(PIN_ATN);
    }

    void SetATN(bool state)
    {
        SetSignal(PIN_ATN, state);
    }

    bool GetACK() const
    {
        return GetSignal(PIN_ACK);
    }

    void SetACK(bool state)
    {
        SetSignal(PIN_ACK, state);
    }

    bool GetRST() const
    {
        return GetSignal(PIN_RST);
    }

    void SetRST(bool state)
    {
        SetSignal(PIN_RST, state);
    }

    bool GetMSG() const
    {
        return GetSignal(PIN_MSG);
    }

    void SetMSG(bool state)
    {
        SetSignal(PIN_MSG, state);
    }

    bool GetCD() const
    {
        return GetSignal(PIN_CD);
    }

    void SetCD(bool state)
    {
        SetSignal(PIN_CD, state);
    }

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

    static const array<BusPhase, 8> phases;

    static const array<string, 11> phase_names;

    bool target_mode = true;

    // The DaynaPort SCSI Link do a short delay in the middle of transfering
    // a packet. This is the number of ns that will be delayed between the
    // header and the actual data.
    static constexpr int DAYNAPORT_SEND_DELAY_NS = 100'000;
};
