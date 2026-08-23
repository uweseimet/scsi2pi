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
    const string &identifier, bool standard_board)
{
#ifdef BOARD_STANDARD
    standard_board = true;
#endif

    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<InProcessBus>(identifier, log_signals);
    }
#if __has_include (<linux/gpio.h>)
    else if (const auto pi_type = RpiBus::GetPiType(); pi_type != RpiBus::PiType::UNKNOWN) {
        auto rpi_bus = make_unique<RpiBus>(pi_type);
        if (standard_board) {
            rpi_bus->SetStandardBoard();
        }
        bus = std::move(rpi_bus);
    }
#endif
    else {
#if __has_include (<linux/gpio.h>)
        spdlog::warn("This platform is not a Raspberry Pi, functionality is limited");
#endif

        bus = make_unique<InProcessBus>(identifier, false);
    }

    return bus->Init(target) ? std::move(bus) : nullptr;
}
