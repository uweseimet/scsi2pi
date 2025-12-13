//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "in_process_bus.h"
#include "shared/s2p_util.h"

using namespace spdlog;
using namespace s2p_util;

InProcessBus::InProcessBus(const string &name, bool l) : in_process_logger(CreateLogger(name)), log_signals(l)
{
    // Log without timestamps
    in_process_logger->set_pattern("[%n] [%^%l%$] %v");
}

void InProcessBus::Reset()
{
    in_process_logger->trace("Resetting bus");

    signals = 0;
    dat = 0;
}

bool InProcessBus::GetSignal(int pin) const
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    scoped_lock lock(signal_lock);

    const bool state = signals & (1 << pin);

    if (log_signals) {
        if (const string &name = GetSignalName(pin); !name.empty()) {
            LogSignal(fmt::format("Getting {0}: {1}", name, state ? "true" : "false"));
        }
    }

    return state;
}

void InProcessBus::SetSignal(int pin, bool state)
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    scoped_lock lock(signal_lock);

    if (log_signals) {
        if (const string &name = GetSignalName(pin); !name.empty()) {
            LogSignal(fmt::format("Setting {0} to {1}", name, state ? "true" : "false"));
        }
    }

    if (state) {
        signals |= (1 << pin);
    }
    else {
        signals &= ~(1 << pin);
    }
}

bool InProcessBus::WaitForSelection()
{
    Sleep( { .tv_sec = 0, .tv_nsec = 10'000'000 });

    return true;
}

void InProcessBus::LogSignal(const string &msg) const
{
    if (msg != last_log_msg) {
        in_process_logger->trace(msg);
        last_log_msg = msg;
    }
}

string InProcessBus::GetSignalName(int pin)
{
    const auto &it = SIGNALS_TO_LOG.find(pin);
    return it != SIGNALS_TO_LOG.end() ? it->second : "";
}
