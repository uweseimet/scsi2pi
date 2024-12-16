//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include "rpi_bus.h"

using namespace std;

class BusFactory
{

public:

    static BusFactory& Instance()
    {
        static BusFactory instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    unique_ptr<Bus> CreateBus(bool, bool, const string&, bool = false);

private:

    BusFactory() = default;

    static RpiBus::PiType CheckForPi();
};
