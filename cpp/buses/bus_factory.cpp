//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <condition_variable>
#include "in_process_bus.h"
#include "rpi_bus.h"

using namespace spdlog;

unique_ptr<Bus> bus_factory::CreateBus(bool target, bool in_process, bool log_signals, const string &identifier)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<InProcessBus>(identifier, log_signals);
    }
    else if (const auto pi_type = RpiBus::GetPiType(); pi_type != RpiBus::PiType::UNKNOWN) {
        bus = make_unique<RpiBus>(pi_type);
    }
    else {
        bus = make_unique<InProcessBus>(identifier, false);
    }

    return bus->Init(target) ? std::move(bus) : nullptr;
}
