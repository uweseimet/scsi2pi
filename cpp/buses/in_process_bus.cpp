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

bool InProcessBus::Init(bool)
{
    // Call super class to avoid logging
    Bus::Reset();

    return true;
}

void InProcessBus::Reset() const
{
    in_process_logger->trace("Resetting bus");

    Bus::Reset();
}

uint8_t InProcessBus::GetDAT() const
{
    scoped_lock lock(signal_lock);

    return Bus::GetDAT();
}

void InProcessBus::SetDAT(uint8_t dat) const
{
    scoped_lock lock(signal_lock);

    uint32_t s = ~GetSignals();
    s &= 0b11111111111111000000001111111111;
    s |= static_cast<uint32_t>(static_cast<byte>(dat)) << PIN_DT0;
    SetSignals(~s);
}

bool InProcessBus::GetSignal(int pin_mask) const
{
    scoped_lock lock(signal_lock);

    const bool state = Bus::GetSignal(pin_mask);

    if (log_signals) {
        if (const string &name = GetSignalName(pin_mask); !name.empty()) {
            LogSignal(fmt::format("Getting {0}: {1}", name, state ? "true" : "false"));
        }
    }

    return state;
}

void InProcessBus::SetSignal(int pin, bool state) const
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    if (log_signals) {
        if (const string &name = GetSignalName(pin); !name.empty()) {
            LogSignal(fmt::format("Setting {0} to {1}", name, state ? "true" : "false"));
        }
    }

    scoped_lock lock(signal_lock);

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
