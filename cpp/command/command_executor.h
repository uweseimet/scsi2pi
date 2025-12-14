//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <spdlog/spdlog.h>

class Bus;
class CommandContext;
class ControllerFactory;
class Device;
class PrimaryDevice;
class StorageDevice;
namespace s2p_interface
{
class PbCommand;
class PbDeviceDefinition;
}

using namespace std;
using namespace spdlog;
using namespace s2p_interface;

class CommandExecutor
{

public:

    CommandExecutor(Bus &b, ControllerFactory &factory, logger &logger)
    : bus(b), controller_factory(factory), s2p_logger(logger)
    {
    }
    ~CommandExecutor() = default;

    const auto& GetReservedIds() const
    {
        return reserved_ids;
    }

    // TODO At least some of these methods and of the protected methods should be private.
    // Currently they are called by the unit tests.

    bool ProcessDeviceCmd(const CommandContext&, const PbDeviceDefinition&, bool);
    bool ProcessCmd(const CommandContext&);
    bool Insert(const CommandContext&, const PbDeviceDefinition&, const shared_ptr<PrimaryDevice>, bool) const;
    void DetachAll() const;
    string SetReservedIds(const string&);
#ifdef BUILD_STORAGE_DEVICE
    bool ValidateImageFile(const CommandContext&, StorageDevice&, const string&) const;
#endif
    bool ValidateDevice(const CommandContext&, const PbDeviceDefinition&) const;
    shared_ptr<PrimaryDevice> CreateDevice(const CommandContext&, const PbDeviceDefinition&) const;
    bool SetBlockSize(const CommandContext&, shared_ptr<PrimaryDevice>, int) const;

    static bool ValidateOperation(const CommandContext&, const PrimaryDevice&);
    static string PrintCommand(const PbCommand&, const PbDeviceDefinition&);
    static bool SetProductData(const CommandContext&, const PbDeviceDefinition&, PrimaryDevice&);

private:

    bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
    bool Detach(const CommandContext&, PrimaryDevice&, bool) const;
    bool Start(PrimaryDevice&) const;
    bool Stop(PrimaryDevice&) const;
    bool Eject(PrimaryDevice&) const;
    bool Protect(PrimaryDevice&) const;
    bool Unprotect(PrimaryDevice&) const;

    bool EnsureLun0(const CommandContext&) const;

    bool SetScsiLevel(const CommandContext&, PrimaryDevice&, int) const;

    void DisplayDeviceInfo(const PrimaryDevice&) const;

    static string GetTypeString(const Device&);
    static string GetIdentifier(const Device&);

    static bool CheckForReservedFile(const CommandContext&, const string&);
    static void SetUpDeviceProperties(shared_ptr<PrimaryDevice>);

    Bus &bus;

    ControllerFactory &controller_factory;

    logger &s2p_logger;

    unordered_set<int> reserved_ids;
};
