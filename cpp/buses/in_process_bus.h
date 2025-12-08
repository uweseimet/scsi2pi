//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include "bus.h"

class InProcessBus : public Bus
{

public:

    ~InProcessBus() override = default;

    static InProcessBus& GetInstance()
    {
        static InProcessBus instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    bool Init(bool) override;
    void Ready() override;

    void CleanUp() override
    {
        // Nothing to do
    }

    void Acquire() const override
    {
        // Nothing to do
    }

    void SetDAT(uint8_t) const override;

    void SetSignal(int, bool) const override;

    uint8_t WaitForSelection() override;

    void WaitNanoSeconds(bool) const override;

    bool IsRaspberryPi() const override
    {
        return false;
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
        // Nothing to do
    }

    void SetDir(bool) const override
    {
        // Nothing to do
    }

    static inline atomic_bool target_enabled;

    mutable mutex write_locker;
};

class DelegatingInProcessBus : public InProcessBus
{

public:

    DelegatingInProcessBus(InProcessBus&, const string&, bool);
    ~DelegatingInProcessBus() override = default;

    void Reset() const override;

    void CleanUp() override
    {
        bus.CleanUp();
    }

    void Acquire() const override
    {
        bus.Acquire();
    }

    bool WaitHandShake(int pin, bool state) const override
    {
        return bus.WaitHandShake(pin, state);
    }

    uint8_t GetDAT() const override
    {
        return bus.GetDAT();
    }
    void SetDAT(uint8_t dat) const override
    {
        bus.SetDAT(dat);
    }

    BusPhase GetPhase() const override
    {
        return bus.GetPhase();
    }

    bool IsPhase(BusPhase phase) const override
    {
        return bus.IsPhase(phase);
    }

    bool GetSignal(int) const override;
    void SetSignal(int, bool) const override;

private:

    void LogSignal(const string&) const;

    static string GetSignalName(int);

    InProcessBus &bus;

    shared_ptr<spdlog::logger> in_process_logger;

    bool log_signals = true;

    // For de-duplicating the signal logging
    mutable string last_log_msg;

    inline static const unordered_map<int, const char*> SIGNALS_TO_LOG = {
        { PIN_BSY, "BSY" },
        { PIN_BSY_MASK, "BSY" },
        { PIN_SEL, "SEL" },
        { PIN_SEL_MASK, "SEL" },
        { PIN_ATN, "ATN" },
        { PIN_ATN_MASK, "ATN" },
        { PIN_RST, "RST" },
        { PIN_RST_MASK, "RST" },
        { PIN_MSG, "MSG" },
        { PIN_MSG_MASK, "MSG" },
        { PIN_CD, "CD" },
        { PIN_CD_MASK, "CD" },
        { PIN_IO, "IO" },
        { PIN_IO_MASK, "IO" }
    };
};
