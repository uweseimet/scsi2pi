//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "buses/in_process_bus.h"

using namespace std;

class BusFactory
{

public:

    BusFactory() = default;
    ~BusFactory() = default;

    unique_ptr<Bus> CreateBus(Bus::mode_e, bool = false);

    bool IsRaspberryPi() const
    {
        return is_raspberry_pi;
    }

private:

    bool CheckForPi();

    // Bus instance shared by initiator and target
    inline static InProcessBus in_process_bus;

    bool is_raspberry_pi = false;

    inline static const string DEVICE_TREE_MODEL_PATH = "/proc/device-tree/model";
};
