//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <mutex>
#include "controllers/controller_factory.h"
#include "devices/storage_device.h"

class CommandContext;

class CommandExecutor
{

public:

    CommandExecutor(Bus &bus, shared_ptr<ControllerFactory> controller_factory)
    : bus(bus), controller_factory(controller_factory)
    {
    }
    ~CommandExecutor() = default;

    // TODO At least some of these methods should be private, currently they are directly called by the unit tests

    auto GetReservedIds() const
    {
        return reserved_ids;
    }

    bool ProcessDeviceCmd(const CommandContext&, const PbDeviceDefinition&, bool);
    bool ProcessCmd(const CommandContext&);
    bool Start(PrimaryDevice&) const;
    bool Stop(PrimaryDevice&) const;
    bool Eject(PrimaryDevice&) const;
    bool Protect(PrimaryDevice&) const;
    bool Unprotect(PrimaryDevice&) const;
    bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
    bool Insert(const CommandContext&, const PbDeviceDefinition&, const shared_ptr<PrimaryDevice>&, bool) const;
    bool Detach(const CommandContext&, PrimaryDevice&, bool) const;
    void DetachAll() const;
    string SetReservedIds(string_view);
    bool ValidateImageFile(const CommandContext&, StorageDevice&, const string&) const;
    bool EnsureLun0(const CommandContext&, const PbCommand&) const;
    bool VerifyExistingIdAndLun(const CommandContext&, int, int) const;
    shared_ptr<PrimaryDevice> CreateDevice(const CommandContext&, const PbDeviceType, int, const string&) const;
    bool SetScsiLevel(const CommandContext&, shared_ptr<PrimaryDevice>, int) const;
    bool SetSectorSize(const CommandContext&, shared_ptr<PrimaryDevice>, int) const;

    mutex& GetExecutionLocker()
    {
        return execution_locker;
    }

    auto GetAllDevices() const
    {
        return controller_factory->GetAllDevices();
    }

    static bool ValidateOperationAgainstDevice(const CommandContext&, const PrimaryDevice&, PbOperation);
    static bool ValidateIdAndLun(const CommandContext&, int, int);
    static bool SetProductData(const CommandContext&, const PbDeviceDefinition&, PrimaryDevice&);
    static string PrintCommand(const PbCommand&, const PbDeviceDefinition&);

private:

    static string GetIdentifier(const Device &device)
    {
        return device.GetTypeString() + " " + to_string(device.GetId()) + ":" + to_string(device.GetLun());
    }

    static void DisplayDeviceInfo(const PrimaryDevice&);
    static bool CheckForReservedFile(const CommandContext&, const string&);
    static void SetUpDeviceProperties(const CommandContext&, shared_ptr<PrimaryDevice>);

    Bus &bus;

    shared_ptr<ControllerFactory> controller_factory;

    mutex execution_locker;

    unordered_set<int> reserved_ids;

    const inline static unordered_set<PbDeviceType> UNIQUE_DEVICE_TYPES = {
        PbDeviceType::SCDP,
        PbDeviceType::SCHS
    };
};
