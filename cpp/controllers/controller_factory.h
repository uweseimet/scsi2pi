//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "buses/bus.h"
#include "base/primary_device.h"
#include "abstract_controller.h"

using namespace std;

class PrimaryDevice;

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
    void DeleteAllControllers();
    AbstractController::shutdown_mode ProcessOnController(int) const;
    shared_ptr<AbstractController> FindController(int) const;
    bool HasController(int) const;
    unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
    bool HasDeviceForIdAndLun(int, int) const;
    shared_ptr<PrimaryDevice> GetDeviceForIdAndLun(int, int) const;

    static int GetIdMax()
    {
        return 8;
    }
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

    // TODO Try to make non-static
    inline static bool is_sasi = false;
};
