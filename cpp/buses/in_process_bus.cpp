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

    // Wait for the in-process target up to 1 s
    const auto now = chrono::steady_clock::now();
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

void InProcessBus::SetDAT(uint8_t dat)
{
    uint32_t s = GetSignals();
    s &= 0b1111111111111000000001111111111;
    s |= static_cast<uint32_t>(static_cast<byte>(dat)) << PIN_DT0;
    SetSignals(s);
}

void InProcessBus::SetControl(int pin, bool state)
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    scoped_lock lock(write_locker);
    if (state) {
        SetSignals(GetSignals() | (1 << pin));
    } else {
        SetSignals(GetSignals() & ~(7 << pin));
    }
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

bool DelegatingInProcessBus::GetControl(int pin) const
{
    const bool state = bus.GetControl(pin);

    if (log_signals && pin != PIN_ACK_MASK && pin != PIN_REQ_MASK && in_process_logger->level() == level::trace) {
        in_process_logger->trace("Getting {0}: {1}", GetSignalName(pin == PIN_ACK_MASK ? PIN_ACK : PIN_REQ),
            state ? "true" : "false");
    }

    return state;
}

void DelegatingInProcessBus::SetControl(int pin, bool state)
{
    if (log_signals && pin != PIN_ACK && pin != PIN_REQ && in_process_logger->level() == level::trace) {
        in_process_logger->trace(" Setting {0} to {1}", GetSignalName(pin), state ? "true" : "false");
    }

    bus.SetControl(pin, state);
}

string DelegatingInProcessBus::GetSignalName(int pin)
{
    const auto &it = SIGNALS.find(pin);
    return it != SIGNALS.end() ? it->second : "????";
}
