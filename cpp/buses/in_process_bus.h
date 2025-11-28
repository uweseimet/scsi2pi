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
    void CleanUp() override;
    void Reset() override;

    void Acquire() override
    {
        // Nothing to do
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
        return GetSignal(PIN_IO_MASK);
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

    uint8_t WaitForSelection() override;

    void WaitBusSettle() const override
    {
        // Nothing to do
    }

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
        // Nothing to do }
    }

    static inline atomic_bool target_enabled;

    mutex write_locker;

    atomic<uint8_t> dat = 0;

    uint32_t signals = 0;
};

class DelegatingInProcessBus : public InProcessBus
{

public:

    DelegatingInProcessBus(InProcessBus&, const string&, bool);
    ~DelegatingInProcessBus() override = default;

    void Reset() override;

    void CleanUp() override
    {
        bus.CleanUp();
    }

    void Acquire() override
    {
        bus.Acquire();
    }

    bool WaitHandshake(int pin, bool state) override
    {
        return bus.WaitHandshake(pin, state);
    }

    uint8_t GetDAT() override
    {
        return bus.GetDAT();
    }
    void SetDAT(uint8_t d) override
    {
        bus.SetDAT(d);
    }

    bool GetSignal(int) const override;
    void SetSignal(int, bool) override;

private:

    static string GetSignalName(int);

    InProcessBus &bus;

    shared_ptr<spdlog::logger> in_process_logger;

    bool log_signals = true;

    inline static const unordered_map<int, const char*> SIGNALS = {
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
