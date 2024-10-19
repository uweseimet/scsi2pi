//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/s2p_util.h"
#include "generated/s2p_interface.pb.h"

using namespace s2p_interface;

// The map used for storing/passing device parameters
using param_map = unordered_map<string, string, s2p_util::StringHash, equal_to<>>;

class Device // NOSONAR The number of fields and methods is justified, the complexity is low
{

public:

    virtual ~Device() = default;

    PbDeviceType GetType() const
    {
        return type;
    }

    bool IsReady() const
    {
        return ready;
    }

    void SetReset(bool b)
    {
        reset = b;
    }

    bool IsProtectable() const
    {
        return protectable;
    }
    void SetProtectable(bool b)
    {
        protectable = b;
    }
    bool IsProtected() const
    {
        return write_protected;
    }
    void SetProtected(bool);
    bool IsReadOnly() const
    {
        return read_only;
    }
    void SetReadOnly(bool b)
    {
        read_only = b;
    }
    bool IsStoppable() const
    {
        return stoppable;
    }
    bool IsStopped() const
    {
        return stopped;
    }
    bool IsRemovable() const
    {
        return removable;
    }
    bool IsRemoved() const
    {
        return removed;
    }
    void SetRemoved(bool b)
    {
        removed = b;
    }
    bool IsLocked() const
    {
        return locked;
    }

    virtual int GetId() const = 0;
    int GetLun() const
    {
        return lun;
    }

    const string& GetVendor() const
    {
        return vendor;
    }
    void SetVendor(const string&);
    const string& GetProduct() const
    {
        return product;
    }
    void SetProduct(const string&, bool = true);
    const string& GetRevision() const
    {
        return revision;
    }
    void SetRevision(const string&);
    string GetPaddedName() const;

    virtual bool SupportsFile() const
    {
        return false;
    }
    bool SupportsParams() const
    {
        return supports_params;
    }
    void SupportsParams(bool b)
    {
        supports_params = b;
    }
    auto GetParams() const
    {
        return params;
    }
    virtual param_map GetDefaultParams() const
    {
        return {};
    }

    bool Start();
    void Stop();
    virtual bool Eject(bool);

protected:

    Device(PbDeviceType, int);

    void SetReady(bool b)
    {
        ready = b;
    }
    bool IsReset() const
    {
        return reset;
    }
    bool IsAttn() const
    {
        return attn;
    }
    void SetAttn(bool b)
    {
        attn = b;
    }

    void SetRemovable(bool b)
    {
        removable = b;
    }
    void SetStoppable(bool b)
    {
        stoppable = b;
    }
    void SetStopped(bool b)
    {
        stopped = b;
    }
    void SetLocked(bool b)
    {
        locked = b;
    }

    string GetParam(const string&) const;
    void SetParams(const param_map&);

private:

    const PbDeviceType type;
    const int lun;

    bool ready = false;
    bool reset = false;
    bool attn = false;

    // Device is protectable/write-protected
    bool protectable = false;
    bool write_protected = false;
    // Device is permanently read-only (e.g. a CD-ROM drive)
    bool read_only = false;

    // Device can be stopped (parked)/is stopped (parked)
    bool stoppable = false;
    bool stopped = false;

    // Medium is removable/removed/locked
    bool removable = false;
    bool removed = false;
    bool locked = false;

    bool supports_params = false;

    // Default device identifier for INQUIRY
    string vendor = "SCSI2Pi";
    string product;
    string revision;

    // The parameters the device was created with
    param_map params;
};
