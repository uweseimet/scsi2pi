//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include "in_process_bus.h"
#ifdef __linux__
#include "pi/rpi_bus.h"
#endif

using namespace spdlog;

unique_ptr<Bus> BusFactory::CreateBus(bool target, bool in_process, const string &identifier, bool log_signals)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::GetInstance(), identifier, log_signals);
    }
#ifdef __linux__
    else if (const auto pi_type = RpiBus::CheckForPi(); pi_type != RpiBus::PiType::UNKNOWN) {
        bus = make_unique<RpiBus>(pi_type);
    }
#endif
    else {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::GetInstance(), identifier, false);
    }

    if (bus->Init(target)) {
        bus->Reset();
        return bus;
    }

    return nullptr;
}
