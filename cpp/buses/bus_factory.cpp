//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <sstream>
#include <fstream>
#include "bus_factory.h"
#include "rpi_bus.h"

using namespace std;

unique_ptr<Bus> BusFactory::CreateBus(Bus::mode_e mode, bool in_process)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(in_process_bus, true);
    }
    else {
        if (CheckForPi()) {
            if (getuid()) {
                spdlog::error("GPIO bus access requires root permissions");
                return nullptr;
            }

            bus = make_unique<RpiBus>();
        } else {
            bus = make_unique<DelegatingInProcessBus>(in_process_bus, false);
        }
    }

    if (bus && bus->Init(mode)) {
        bus->Reset();
    }

    return bus;
}

bool BusFactory::CheckForPi()
{
    ifstream in(DEVICE_TREE_MODEL_PATH);
    if (in.fail()) {
        spdlog::info("This platform does not appear to be a Raspberry Pi, functionality is limited");
        return false;
    }

    stringstream s;
    s << in.rdbuf();
    const string model = s.str();

    if (model.starts_with("Raspberry Pi") && !model.starts_with("Raspberry Pi 5")) {
        spdlog::info("Detected {}", model);
        is_raspberry_pi = true;
        return true;
    }

    spdlog::error("Unsupported Raspberry Pi model '{}', functionality may be limited", model);

    return false;
}

