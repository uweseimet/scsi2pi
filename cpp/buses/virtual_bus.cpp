//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <buses/virtual_bus.h>
#include "shared/s2p_util.h"

using namespace spdlog;
using namespace s2p_util;

VirtualBus::VirtualBus(const string &name, bool l) : virtual_bus_logger(CreateLogger(name)), log_signals(l)
{
    // Log without timestamps
    virtual_bus_logger->set_pattern("[%n] [%^%l%$] %v");
}

void VirtualBus::Reset() const
{
    virtual_bus_logger->trace("Resetting bus");

    Bus::Reset();
}

void VirtualBus::CleanUp()
{
    {
        scoped_lock lock(sel_lock);
        selected = true;
    }
    sel.notify_one();

    Bus::CleanUp();
}

void VirtualBus::SetDAT(uint8_t dat) const
{
    scoped_lock lock(signal_lock);

    uint32_t s = ~GetSignals();
    s &= ~DATA_BITS_MASK;
    s |= static_cast<uint32_t>(dat) << PIN_DT0;
    SetSignals(~s);
}

bool VirtualBus::GetSignal(int pin_mask) const
{
    scoped_lock lock(signal_lock);

    const bool state = Bus::GetSignal(pin_mask);

    if (log_signals) {
        if (const string &name = GetSignalName(pin_mask); !name.empty()) {
            LogSignal(fmt::format("Getting {}: {}", name, state ? "true" : "false"));
        }
    }

    return state;
}

void VirtualBus::SetSignal(int pin, bool state) const
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    scoped_lock lock(signal_lock);

    if (log_signals) {
        if (const string &name = GetSignalName(1 << pin); !name.empty()) {
            LogSignal(fmt::format("Setting {} to {}", name, state ? "true" : "false"));
        }
    }

    if (state) {
        SetSignals(GetSignals() & ~(1 << pin));
    } else {
        SetSignals(GetSignals() | (1 << pin));
    }

    if (pin == PIN_SEL && state) {
        scoped_lock guard(sel_lock);
        selected = true;
        sel.notify_one();
    }
}

uint8_t VirtualBus::WaitForSelection()
{
    {
        unique_lock lock(sel_lock);

        if (!selected) {
            sel.wait(lock, []
                {
                    return selected;
                });
        }

        selected = false;
    }

    return GetSelection();
}

void VirtualBus::LogSignal(const string &msg) const
{
    scoped_lock guard(last_log_msg_mutex);

    if (msg != last_log_msg) {
        virtual_bus_logger->trace(msg);
        last_log_msg = msg;
    }
}

string VirtualBus::GetSignalName(int pin_mask)
{
    const auto &it = SIGNALS_TO_LOG.find(pin_mask);
    return it != SIGNALS_TO_LOG.end() ? it->second : "";
}
