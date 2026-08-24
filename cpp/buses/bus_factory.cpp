//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <spdlog/spdlog.h>
#include "in_process_bus.h"
#if __has_include (<linux/gpio.h>)
#include "rpi_bus.h"
#endif

unique_ptr<Bus> bus_factory::CreateBus(bool target, bool in_process, bool log_signals,
    const string &identifier, [[maybe_unused]] bool standard_board)
{
    auto make_initialized = [target](unique_ptr<Bus> bus) {
        return (bus && bus->Init(target)) ? std::move(bus) : nullptr;
    };

    if (in_process) {
        return make_initialized(make_unique<InProcessBus>(identifier, log_signals));
    }

#if __has_include (<linux/gpio.h>)
    if (const auto pi_type = RpiBus::GetPiType(); pi_type != RpiBus::PiType::UNKNOWN) {
        constexpr bool override_standard =
#ifdef BOARD_STANDARD
            true;
#else
            false;
#endif

        auto bus = make_unique<RpiBus>(pi_type, override_standard || standard_board);
        return make_initialized(std::move(bus));    }
#endif

    spdlog::warn("This platform is not a Raspberry Pi, functionality is limited");

    // Fall back to the in-process bus
    return make_initialized(make_unique<InProcessBus>(identifier, false));
}
