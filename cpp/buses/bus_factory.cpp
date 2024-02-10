//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <sstream>
#include <fstream>
#include <spdlog/spdlog.h>
#include "rpi_bus.h"
#include "in_process_bus.h"
#include "bus_factory.h"

using namespace spdlog;

unique_ptr<Bus> BusFactory::CreateBus(bool target, bool in_process)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), true);
    }
    else {
        if (CheckForPi()) {
            if (getuid()) {
                error("GPIO bus access requires root permissions");
                return nullptr;
            }

            bus = make_unique<RpiBus>();
        } else {
            bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), false);
        }
    }

    if (bus && bus->Init(target)) {
        bus->Reset();
    }

    return bus;
}

bool BusFactory::CheckForPi()
{
    ifstream in("/proc/device-tree/model");
    if (in.fail()) {
        info("This platform does not appear to be a Raspberry Pi, functionality is limited");
        return false;
    }

    stringstream s;
    s << in.rdbuf();
    const string model = s.str();

    if (model.starts_with("Raspberry Pi") && !model.starts_with("Raspberry Pi 5")) {
        is_raspberry_pi = true;
        return true;
    }

    warn("Unsupported Raspberry Pi model '{}', functionality is limited", model);

    return false;
}

