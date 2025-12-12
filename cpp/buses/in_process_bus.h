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

    InProcessBus(const string&, bool);
    ~InProcessBus() override = default;

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

    bool IsRaspberryPi() const override
    {
        return false;
    }

private:

    void LogSignal(const string&) const;

    void DisableIRQ() override
    {
        // Nothing to do
    }
    void EnableIRQ() override
    {
        // Nothing to do }
    }

    static string GetSignalName(int);

    static inline atomic_bool target_enabled;

    mutex write_locker;

    inline static atomic<uint8_t> dat = 0;

    inline static array<bool, 28> signals = { };

    shared_ptr<spdlog::logger> in_process_logger;

    bool log_signals = true;

    // For de-duplicating the signal logging
    mutable string last_log_msg;

    // To prevent competing signal changes and overlapping logs
    inline static mutex signal_lock;

    // TODO Why does an unordered_map often cause a segfault when calling SIGNALS_TO_LOG.find()?
    inline static const map<int, const char*> SIGNALS_TO_LOG = {
        { PIN_BSY, "BSY" },
        { PIN_SEL, "SEL" },
        { PIN_ATN, "ATN" },
        { PIN_RST, "RST" },
        { PIN_MSG, "MSG" },
        { PIN_CD, "CD" },
        { PIN_IO, "IO" }
    };
};
