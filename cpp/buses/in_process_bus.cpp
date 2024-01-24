//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "in_process_bus.h"

using namespace spdlog;

void InProcessBus::Reset()
{
    signals = { };

    dat = 0;
}

bool InProcessBus::GetSignal(int pin) const
{
    return signals[pin];
}

void InProcessBus::SetSignal(int pin, bool state)
{
    scoped_lock lock(write_locker);
    signals[pin] = state;
}

bool InProcessBus::WaitSignal(int pin, bool state)
{
    const auto now = chrono::steady_clock::now();

    do {
        if (signals[pin] == state) {
            return true;
        }

        if (signals[PIN_RST]) {
            return false;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    return false;
}

bool InProcessBus::WaitForSelection()
{
    // Busy waiting cannot be avoided
    const timespec ts = { .tv_sec = 0, .tv_nsec = 10'000'000 };
    nanosleep(&ts, nullptr);

    return true;
}

void DelegatingInProcessBus::Reset()
{
    trace(GetMode() + ": Resetting bus");

    bus.Reset();
}

bool DelegatingInProcessBus::GetSignal(int pin) const
{
    const bool state = bus.GetSignal(pin);

    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && get_level() == level::trace) {
        trace(GetMode() + ": Getting " + GetSignalName(pin) + (state ? ": true" : ": false"));
    }

    return state;
}

void DelegatingInProcessBus::SetSignal(int pin, bool state)
{
    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && get_level() == level::trace) {
        trace(GetMode() + ": Setting " + GetSignalName(pin) + " to " + (state ? "true" : "false"));
    }

    bus.SetSignal(pin, state);
}

bool DelegatingInProcessBus::WaitSignal(int pin, bool state)
{
    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && get_level() == level::trace) {
        trace(GetMode() + ": Waiting for " + GetSignalName(pin) + " to become " + (state ? "true" : "false"));
    }

    return bus.WaitSignal(pin, state);
}

string DelegatingInProcessBus::GetSignalName(int pin) const
{
    const auto &it = SIGNALS.find(pin);
    return it != SIGNALS.end() ? it->second : "????";
}
