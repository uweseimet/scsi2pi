//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <unordered_set>
#include "buses/bus.h"
#include "phase_handler.h"
#include "base/device_logger.h"
#include "base/memory_util.h"
#include "base/s2p_defs.h"

class PrimaryDevice;

class AbstractController : public PhaseHandler
{

public:

    AbstractController(Bus&, int);
    ~AbstractController() override = default;

    virtual void Error(sense_key, asc, status_code) = 0;

    virtual int GetEffectiveLun() const = 0;

    virtual void Reset();

    void CleanUp() const;

    int GetInitiatorId() const
    {
        return initiator_id;
    }

    void ScheduleShutdown(shutdown_mode mode)
    {
        sh_mode = mode;
    }

    int GetTargetId() const
    {
        return target_id;
    }

    auto GetLunCount() const
    {
        return luns.size();
    }

    unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
    shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
    bool AddDevice(shared_ptr<PrimaryDevice>);
    bool RemoveDevice(PrimaryDevice&);
    shutdown_mode ProcessOnController(int);

    void CopyToBuffer(const void*, size_t);
    auto& GetBuffer()
    {
        return buffer;
    }
    auto GetStatus() const
    {
        return status;
    }
    auto GetChunkSize() const
    {
        return chunk_size;
    }
    auto GetCurrentLength() const
    {
        return current_length;
    }
    void SetCurrentLength(int);
    void SetTransferSize(int, int);
    auto& GetCdb() const
    {
        return cdb;
    }

protected:

    virtual bool Process() = 0;

    Bus& GetBus() const
    {
        return bus;
    }

    void SetCdbByte(int index, int value)
    {
        cdb[index] = value;
    }

    bool UpdateTransferSize()
    {
        total_length -= chunk_size;
        return total_length != 0;
    }

    void SetStatus(status_code s)
    {
        status = s;
    }

    auto GetOffset() const
    {
        return offset;
    }
    void ResetOffset()
    {
        offset = 0;
    }
    void UpdateOffsetAndLength()
    {
        offset += current_length;
        current_length = 0;
    }

    void LogTrace(const string &s) const
    {
        device_logger.Trace(s);
    }
    void LogDebug(const string &s) const
    {
        device_logger.Debug(s);
    }
    void LogWarn(const string &s) const
    {
        device_logger.Warn(s);
    }

private:

    array<int, 16> cdb = { };

    // Shared ransfer data buffer, dynamically resized
    inline static vector<uint8_t> buffer = vector<uint8_t>(4096);
    // Transfer offset
    int offset = 0;
    // Total number of bytes to be transferred
    int total_length = 0;
    // Remaining bytes to be transferred in a single handshake cycle
    int current_length = 0;
    // The number of bytes to be transferred with the current handshake cycle
    int chunk_size = 0;

    status_code status = status_code::good;

    Bus &bus;

    DeviceLogger device_logger;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    static constexpr int UNKNOWN_INITIATOR_ID = -1;

    // The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
    int initiator_id = UNKNOWN_INITIATOR_ID;

    shutdown_mode sh_mode = shutdown_mode::none;
};
