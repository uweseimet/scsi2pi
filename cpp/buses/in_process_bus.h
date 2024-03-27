//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <mutex>
#include <atomic>
#include "bus.h"

class InProcessBus : public Bus
{

public:

    ~InProcessBus() override = default;

    static InProcessBus& Instance()
    {
        static InProcessBus instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    bool Init(bool) override;
    void CleanUp() override;
    void Reset() override;

    uint32_t Acquire() override
    {
        return dat;
    }

    void SetBSY(bool state) override
    {
        SetSignal(PIN_BSY, state);
    }

    void SetSEL(bool state) override
    {
        SetSignal(PIN_SEL, state);
    }

    bool GetIO() override
    {
        return GetSignal(PIN_IO);
    }
    void SetIO(bool state) override
    {
        SetSignal(PIN_IO, state);
    }

    uint8_t GetDAT() override
    {
        return dat;
    }
    void SetDAT(uint8_t d) override
    {
        dat = d;
    }

    bool GetSignal(int) const override;
    void SetSignal(int, bool) override;

    bool WaitForSelection() override;

    void WaitBusSettle() const override
    {
        // Nothing to do
    }

protected:

    InProcessBus() = default;

private:

    void DisableIRQ() override
    {
        // Nothing to do
    }
    void EnableIRQ() override
    {
        // Nothing to do }
    }

    static inline atomic_bool target_enabled;

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

    bool WaitSignal(int pin, bool state) override
    {
        return bus.WaitSignal(pin, state);
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
