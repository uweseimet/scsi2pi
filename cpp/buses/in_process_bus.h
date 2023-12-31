//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "buses/gpio_bus.h"
#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <mutex>
#include <atomic>

class InProcessBus : public GpioBus
{

public:

    InProcessBus() = default;
    ~InProcessBus() override = default;

    void Reset() override;

    void CleanUp() override
    {
        // Nothing to do
    }

    uint32_t Acquire() override
    {
        return dat;
    }

    bool GetBSY() const override
    {
        return GetSignal(PIN_BSY);
    }
    void SetBSY(bool state) override
    {
        SetSignal(PIN_BSY, state);
    }
    bool GetSEL() const override
    {
        return GetSignal(PIN_SEL);
    }
    void SetSEL(bool state) override
    {
        SetSignal(PIN_SEL, state);
    }
    bool GetATN() const override
    {
        return GetSignal(PIN_ATN);
    }
    void SetATN(bool state) override
    {
        SetSignal(PIN_ATN, state);
    }
    bool GetACK() const override
    {
        return GetSignal(PIN_ACK);
    }
    void SetACK(bool state) override
    {
        SetSignal(PIN_ACK, state);
    }
    bool GetRST() const override
    {
        return GetSignal(PIN_RST);
    }
    void SetRST(bool state) override
    {
        SetSignal(PIN_RST, state);
    }
    ;
    bool GetMSG() const override
    {
        return GetSignal(PIN_MSG);
    }
    ;
    void SetMSG(bool state) override
    {
        SetSignal(PIN_MSG, state);
    }
    ;
    bool GetCD() const override
    {
        return GetSignal(PIN_CD);
    }
    void SetCD(bool state) override
    {
        SetSignal(PIN_CD, state);
    }
    bool GetIO() override
    {
        return GetSignal(PIN_IO);
    }
    void SetIO(bool state) override
    {
        SetSignal(PIN_IO, state);
    }
    bool GetREQ() const override
    {
        return GetSignal(PIN_REQ);
    }
    void SetREQ(bool state) override
    {
        SetSignal(PIN_REQ, state);
    }

    bool WaitSignal(int, bool) override;

    bool WaitREQ(bool state) override
    {
        return WaitSignal(PIN_REQ, state);
    }

    bool WaitACK(bool state) override
    {
        return WaitSignal(PIN_ACK, state);
    }

    uint8_t GetDAT() override
    {
        return dat;
    }
    void SetDAT(uint8_t d) override
    {
        dat = d;
    }

    bool GetSignal(int pin) const override;
    void SetSignal(int, bool) override;

    bool WaitForSelection() override;

private:

    void DisableIRQ() override
    {
        // Nothing to do
    }
    void EnableIRQ() override
    {
        // Nothing to do }
    }

    void SetControl(int, bool) override
    {
        assert(false);
    }
    void SetMode(int, int) override
    {
        assert(false);
    }
    void PinConfig(int, int) override
    {
        assert(false);
    }
    void PullConfig(int, int) override
    {
        assert(false);
    }
    void PinSetSignal(int, bool) override
    {
        assert(false);
    }

    mutex write_locker;

    atomic<uint8_t> dat = 0;

    array<bool, 28> signals = { };
};

class DelegatingInProcessBus : public InProcessBus
{

public:

    DelegatingInProcessBus(InProcessBus &b, bool l) : bus(b), log_signals(l)
    {
    }
    ~DelegatingInProcessBus() override = default;

    void Reset() override;

    void CleanUp() override
    {
        bus.CleanUp();
    }

    uint32_t Acquire() override
    {
        return bus.Acquire();
    }

    bool WaitREQ(bool state) override
    {
        return bus.WaitSignal(PIN_REQ, state);
    }

    bool WaitACK(bool state) override
    {
        return bus.WaitSignal(PIN_ACK, state);
    }

    uint8_t GetDAT() override
    {
        return bus.GetDAT();
    }
    void SetDAT(uint8_t dat) override
    {
        bus.SetDAT(dat);
    }

    bool GetSignal(int) const override;
    void SetSignal(int, bool) override;
    bool WaitSignal(int, bool) override;

private:

    string GetMode() const
    {
        return IsTarget() ? "target" : "initiator";
    }

    string GetSignalName(int) const;

    InProcessBus &bus;

    bool log_signals = true;

    inline static const unordered_map<int, string> SIGNALS {
        { PIN_BSY, "BSY" },
        { PIN_SEL, "SEL" },
        { PIN_ATN, "ATN" },
        { PIN_ACK, "ACK" },
        { PIN_RST, "RST" },
        { PIN_MSG, "MSG" },
        { PIN_CD, "CD" },
        { PIN_IO, "IO" },
        { PIN_REQ, "REQ" }
    };
};
