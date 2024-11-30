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
#include "phase_handler.h"
#include "script_generator.h"
#include "buses/bus.h"
#include "base/device_logger.h"
#include "base/s2p_defs.h"
#include "shared/memory_util.h"

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

    void SetScriptGenerator(shared_ptr<ScriptGenerator>);

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
    auto& GetBuffer() const
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
    auto GetTotalLength() const
    {
        return total_length;
    }
    auto GetRemainingLength() const
    {
        return remaining_length;
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

    void AddCdbToScript();
    void AddDataToScript(span<uint8_t>) const;

    virtual bool Process() = 0;

    Bus& GetBus() const
    {
        return bus;
    }

    void SetCdbByte(int index, int value)
    {
        cdb[index] = value;
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

    bool UpdateTransferSize();
    void UpdateOffsetAndLength();

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

    // Shared transfer data buffer, dynamically resized
    inline static auto buffer = vector<uint8_t>(512);
    // Transfer offset
    int offset = 0;
    // Total bytes to transfer
    int total_length = 0;
    // Total remaining bytes to be transferred, updated during the transfer
    int remaining_length = 0;
    // Remaining bytes to be transferred in a single handshake cycle
    int current_length = 0;
    // The number of bytes to be transferred with the current handshake cycle
    int chunk_size = 0;

    status_code status = status_code::good;

    Bus &bus;

    DeviceLogger device_logger;

    shared_ptr<ScriptGenerator> script_generator;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    static constexpr int UNKNOWN_INITIATOR_ID = -1;

    // The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
    int initiator_id = UNKNOWN_INITIATOR_ID;

    shutdown_mode sh_mode = shutdown_mode::none;
};
