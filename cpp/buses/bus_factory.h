//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "bus.h"

using namespace std;

class BusFactory
{

public:

    static BusFactory& Instance()
    {
        static BusFactory instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    unique_ptr<Bus> CreateBus(bool, bool);

    bool IsRaspberryPi() const
    {
        return is_raspberry_pi;
    }

private:

    BusFactory() = default;

    bool CheckForPi();

    bool is_raspberry_pi = false;
};
