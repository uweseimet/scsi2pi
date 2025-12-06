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

bool InProcessBus::Init(bool target)
{
    if (!Bus::Init(target)) {
        return false;
    }

    if (target) {
        return true;
    }

    const auto now = chrono::steady_clock::now();

    // Wait for the target up to 1 s
    do {
        if (target_enabled) {
            return true;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 1);

    return false;
}

void InProcessBus::CleanUp()
{
    // Signal the client that s2p is ready
    if (IsTarget()) {
        target_enabled = true;
    }
}

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

bool InProcessBus::WaitForSelection()
{
    // Busy waiting cannot be avoided
    const timespec ts = { .tv_sec = 0, .tv_nsec = 10'000'000 };
    nanosleep(&ts, nullptr);

    return true;
}

DelegatingInProcessBus::DelegatingInProcessBus(InProcessBus &b, const string &name, bool l) : bus(b), in_process_logger(
    CreateLogger(name)), log_signals(l)
{
    // Log without timestamps
    in_process_logger->set_pattern("[%^%l%$] [%n] %v");
}

void DelegatingInProcessBus::Reset()
{
    in_process_logger->trace("Resetting bus");

    bus.Reset();
}

bool DelegatingInProcessBus::GetSignal(int pin) const
{
    const bool state = bus.GetSignal(pin);

    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && in_process_logger->level() == level::trace) {
        in_process_logger->trace("Getting {0}: {1}", GetSignalName(pin), state ? "true" : "false");
    }

    return state;
}

void DelegatingInProcessBus::SetSignal(int pin, bool state)
{
    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && in_process_logger->level() == level::trace) {
        in_process_logger->trace("Setting {0} to {1}", GetSignalName(pin), state ? "true" : "false");
    }

    bus.SetSignal(pin, state);
}

string DelegatingInProcessBus::GetSignalName(int pin)
{
    const auto &it = SIGNALS.find(pin);
    return it != SIGNALS.end() ? it->second : "????";
}
