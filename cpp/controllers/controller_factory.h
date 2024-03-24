//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include "base/primary_device.h"
#include "abstract_controller.h"

class ControllerFactory
{

public:

    explicit ControllerFactory(bool b = false)
    {
        is_sasi = b;
    }
    ~ControllerFactory() = default;

    bool AttachToController(Bus&, int, shared_ptr<PrimaryDevice>);
    bool DeleteController(const AbstractController&);
    bool DeleteAllControllers();
    AbstractController::shutdown_mode ProcessOnController(int) const;
    shared_ptr<AbstractController> FindController(int) const;
    bool HasController(int) const;
    unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
    bool HasDeviceForIdAndLun(int, int) const;
    shared_ptr<PrimaryDevice> GetDeviceForIdAndLun(int, int) const;

    static int GetLunMax()
    {
        return is_sasi ? GetSasiLunMax() : GetScsiLunMax();
    }
    static int GetScsiLunMax()
    {
        return 32;
    }
    static int GetSasiLunMax()
    {
        return 2;
    }

private:

    shared_ptr<AbstractController> CreateController(Bus&, int) const;

    // Controllers mapped to their device IDs
    unordered_map<int, shared_ptr<AbstractController>> controllers;

    inline static bool is_sasi = false;
};
