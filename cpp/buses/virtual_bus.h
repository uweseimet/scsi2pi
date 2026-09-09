//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>
#include "bus.h"

class VirtualBus final : public Bus
{

public:

    VirtualBus(const string&, bool);

    void CleanUp() override;

    void Reset() const override;

    bool GetSignal(int) const override;
    void SetSignal(int, bool) const override;

private:

    bool IsRaspberryPi() const override
    {
        return false;
    }

    string SetUp(bool) override;

    void LogSignal(const string&) const;

    void Acquire() const override
    {
        // Nothing to do
    }

    void SetDataDirIn(bool) const override
    {
        // Nothing to do
    }

    uint8_t GetDAT() const override;
    void SetDAT(uint8_t) const override;

    BusPhase GetPhase() const override;
    bool IsPhase(BusPhase phase) const override;

    void WaitNanoSeconds(bool) const override
    {
        // Nothing to do
    }

    uint8_t WaitForSelection() override;

    static string GetSignalName(int);

    shared_ptr<spdlog::logger> virtual_bus_logger;

    bool log_signals = true;

    // For de-duplicating the signal logging
    mutable string last_log_msg;
    mutable mutex last_log_msg_mutex;

    // To prevent competing signal changes and overlapping logs
    inline static mutex signal_lock;

    // For simulating the selection event, avoids busy waiting
    inline static mutex sel_lock;
    inline static condition_variable sel;
    inline static bool selected = false;

    inline static constexpr uint32_t DATA_BITS_MASK = 0xffu << PIN_DT0;

    inline static const unordered_map<int, const char*> SIGNALS_TO_LOG = {
        { PIN_BSY_MASK, "BSY" },
        { PIN_SEL_MASK, "SEL" },
        { PIN_ATN_MASK, "ATN" },
        { PIN_RST_MASK, "RST" },
        { PIN_MSG_MASK, "MSG" },
        { PIN_CD_MASK, "CD" },
        { PIN_IO_MASK, "IO" }
    };
};
