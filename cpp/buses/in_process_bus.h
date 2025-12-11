//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include "bus.h"

class InProcessBus final : public Bus
{

public:

    InProcessBus(const string&, bool);
    ~InProcessBus() override = default;

    void Reset() const override;

    bool SetUp(bool) override
    {
        // Nothing to do
        return true;
    }

    void CleanUp() override
    {
        // Nothing to do
    }

    void Acquire() const override
    {
        // Nothing to do
    }

    void SetDAT(uint8_t) const override;

    bool GetSignal(int) const override;
    void SetSignal(int, bool) const override;

    uint8_t WaitForSelection() override;

    void WaitNanoSeconds(bool) const override
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
        // Nothing to do
    }

    void SetDir(bool) const override
    {
        // Nothing to do
    }

    static string GetSignalName(int);

    shared_ptr<spdlog::logger> in_process_logger;

    bool log_signals = true;

    // For de-duplicating the signal logging
    mutable string last_log_msg;

    // To prevent competing signal changes and overlapping logs
    inline static mutex signal_lock;

    // TODO Why does an unordered_map often cause a segfault when calling SIGNALS_TO_LOG.find()?
    inline static const map<int, const char*> SIGNALS_TO_LOG = {
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
