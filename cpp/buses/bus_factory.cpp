//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include "in_process_bus.h"
#include "pi/rpi_bus.h"

using namespace spdlog;

unique_ptr<Bus> bus_factory::CreateBus(bool target, bool in_process, const string &identifier, bool log_signals)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<InProcessBus>(identifier, log_signals);
    }
    else if (RpiBus::GetPiType() != RpiBus::PiType::UNKNOWN) {
        bus = make_unique<RpiBus>();
    }
    else {
        bus = make_unique<InProcessBus>(identifier, false);
    }

    return bus->Init(target) ? std::move(bus) : nullptr;
}
