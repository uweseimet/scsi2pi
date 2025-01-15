//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include "in_process_bus.h"

using namespace spdlog;

unique_ptr<Bus> BusFactory::CreateBus(bool target, bool in_process, const string &identifier, bool log_signals)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), identifier, log_signals);
    }
    else if (const auto pi_type = CheckForPi(); pi_type != RpiBus::PiType::UNKNOWN) {
        bus = make_unique<RpiBus>(pi_type);
    }
    else {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), identifier, false);
    }

    if (bus->Init(target)) {
        bus->Reset();
        return bus;
    }

    return nullptr;
}

RpiBus::PiType BusFactory::CheckForPi()
{
    ifstream in("/proc/device-tree/model");
    if (!in) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::UNKNOWN;
    }

    stringstream s;
    s << in.rdbuf();
    const string &model = s.str();

    if (!model.starts_with("Raspberry Pi ") || model.size() < 13) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::UNKNOWN;
    }

    const int type = model.find("Zero") != string::npos ? 1 : model.substr(13, 1)[0] - '0';
    if (type <= 0 || type > 4) {
        warn("Unsupported Raspberry Pi model '{}', functionality is limited", model);
        return RpiBus::PiType::UNKNOWN;
    }

    return static_cast<RpiBus::PiType>(type);
}

