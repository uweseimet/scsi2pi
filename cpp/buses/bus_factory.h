//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include "bus.h"

class BusFactory final
{

public:

    BusFactory(const BusFactory&) = delete;
    BusFactory& operator=(const BusFactory&) = delete;

    static BusFactory& GetInstance()
    {
        static BusFactory instance; // NOSONAR Singleton with mutable internal state
        return instance;
    }

    unique_ptr<Bus> CreateBus(bool, const string&, bool = false, bool = false);

    void EnableVirtualBus(bool = false);

private:

    BusFactory() = default;

    bool virtual_bus = false;

    bool log_signals = false;
};
