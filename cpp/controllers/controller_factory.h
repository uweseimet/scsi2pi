//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include "base/s2p_defs.h"
#include "shared/s2p_formatter.h"
#include "script_generator.h"

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

    bool SetScriptFile(const string&);

    void SetFormatLimit(int limit)
    {
        formatter.SetLimit(limit);
    }

private:

    S2pFormatter formatter;

    // Controllers mapped to their target IDs
    unordered_map<int, shared_ptr<AbstractController>> controllers;

    shared_ptr<ScriptGenerator> script_generator;
};
