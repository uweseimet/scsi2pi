//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "shared/s2p_defs.h"
#include "shared/s2p_formatter.h"

class Bus;
class AbstractController;
class PrimaryDevice;
class ScriptGenerator;

using namespace std;

class ControllerFactory
{

public:

    bool AttachToController(Bus&, int, shared_ptr<PrimaryDevice>);
    bool DeleteController(const AbstractController&);
    bool DeleteAllControllers();
    ShutdownMode ProcessOnController(int) const;
    bool HasController(int) const;

    unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
    shared_ptr<PrimaryDevice> GetDeviceForIdAndLun(int, int) const;

    bool SetScriptFile(const string&);

    void SetFormatLimit(int limit)
    {
        formatter.SetLimit(limit);
    }

    void SetLogLevel(int id, int lun, spdlog::level::level_enum);
    void SetLogPattern(string_view);

private:

    S2pFormatter formatter;

    // Controllers mapped to their target IDs
    unordered_map<int, shared_ptr<AbstractController>> controllers;

    shared_ptr<ScriptGenerator> script_generator;

    spdlog::level::level_enum log_level = spdlog::get_level();
    string log_pattern = "%n [%^%l%$] %v";
};
