//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <spdlog/spdlog.h>
#if __has_include (<linux/gpio.h>)
#include "pi5_bus.h"
#include "rpi_bus.h"
#endif
#include "virtual_bus.h"

unique_ptr<Bus> BusFactory::CreateBus(Bus::BusProperties bus_properties, const string &identifier)
{
    auto make_initialized = [bus_properties](unique_ptr<Bus> bus) {
        return (bus && bus->Init(bus_properties.target_mode)) ? std::move(bus) : nullptr;
    };

    if (virtual_bus) {
        return make_initialized(make_unique<VirtualBus>(identifier, log_signals));
    }

#if __has_include (<linux/gpio.h>)
    if (const auto pi_type = RpiBus::GetPiType(); pi_type != RpiBus::PiType::UNKNOWN) {
#ifdef BOARD_STANDARD
            bus_properties.standard_board = true;
#else
            bus_properties.standard_board = false;
#endif

        if (pi_type == RpiBus::PiType::PI_5) {
            return make_initialized(make_unique<Pi5Bus>(bus_properties));
        }

        auto bus = make_unique<RpiBus>(pi_type, bus_properties);
        return make_initialized(std::move(bus));
    }
#else
    spdlog::warn("This platform is not a Raspberry Pi running Linux, functionality is limited");
#endif

    // Fall back to the virtual bus
    return make_initialized(make_unique<VirtualBus>(identifier, false));
}

void BusFactory::EnableVirtualBus(bool l)
{
    virtual_bus = true;

    log_signals = l;
}
