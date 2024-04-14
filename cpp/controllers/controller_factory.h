//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_set>
#include <unordered_map>
#include <memory>
#include "base/s2p_defs.h"

class Bus;
class AbstractController;
class PrimaryDevice;

using namespace std;

class ControllerFactory
{

public:

    bool AttachToController(Bus&, int, shared_ptr<PrimaryDevice>);
    bool DeleteController(const AbstractController&);
    bool DeleteAllControllers();
    shutdown_mode ProcessOnController(int) const;
    bool HasController(int) const;
    unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
    shared_ptr<PrimaryDevice> GetDeviceForIdAndLun(int, int) const;

private:

    // Controllers mapped to their target IDs
    unordered_map<int, shared_ptr<AbstractController>> controllers;
};
