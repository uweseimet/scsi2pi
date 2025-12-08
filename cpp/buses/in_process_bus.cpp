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

    // Wait for the in-process bus target up to 1 s
    const auto now = chrono::steady_clock::now();
    do {
        if (target_enabled) {
            return true;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 1);

    return false;
}

void InProcessBus::Ready()
{
    assert(IsTarget());

    // Signal the in-process bus client that the in-process s2p is ready
    target_enabled = true;
}

void InProcessBus::SetDAT(uint8_t dat) const
{
    uint32_t s = ~GetSignals();
    s &= 0b11111111111111000000001111111111;
    s |= static_cast<uint32_t>(static_cast<byte>(dat)) << PIN_DT0;
    SetSignals(~s);
}

void InProcessBus::SetSignal(int pin, bool state) const
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    scoped_lock lock(write_locker);
    if (state) {
        SetSignals(GetSignals() & ~(1 << pin));
    } else {
        SetSignals(GetSignals() | (1 << pin));
    }
}

uint8_t InProcessBus::WaitForSelection()
{
    // Busy waiting cannot be avoided
    const timespec ts = { .tv_sec = 0, .tv_nsec = 10'000'000 };
    nanosleep(&ts, nullptr);

    return GetSelection();
}

void InProcessBus::WaitNanoSeconds(bool) const
{
    // Wait a bus settle delay
    const timespec ts = { .tv_sec = 0, .tv_nsec = 400 };
    nanosleep(&ts, nullptr);
}

DelegatingInProcessBus::DelegatingInProcessBus(InProcessBus &b, const string &name, bool l) : bus(b), in_process_logger(
    CreateLogger(name)), log_signals(l)
{
    // Log without timestamps
    in_process_logger->set_pattern("[%n] [%^%l%$] %v");
}

void DelegatingInProcessBus::Reset() const
{
    in_process_logger->trace("Resetting bus");

    bus.Reset();
}

bool DelegatingInProcessBus::GetSignal(int pin_mask) const
{
    const bool state = bus.GetSignal(pin_mask);

    if (log_signals) {
        if (const string &name = GetSignalName(pin_mask); !name.empty()) {
            LogSignal(fmt::format("Getting {0}: {1}", name, state ? "true" : "false"));
        }
    }

    return state;
}

void DelegatingInProcessBus::SetSignal(int pin, bool state) const
{
    if (log_signals) {
        if (const string &name = GetSignalName(pin); !name.empty()) {
            LogSignal(fmt::format("Setting {0} to {1}", name, state ? "true" : "false"));
        }
    }

    bus.SetSignal(pin, state);
}

void DelegatingInProcessBus::LogSignal(const string &msg) const
{
    if (msg != last_log_msg) {
        in_process_logger->trace(msg);
        last_log_msg = msg;
    }
}

string DelegatingInProcessBus::GetSignalName(int pin)
{
    const auto &it = SIGNALS_TO_LOG.find(pin);
    return it != SIGNALS_TO_LOG.end() ? it->second : "";
}
