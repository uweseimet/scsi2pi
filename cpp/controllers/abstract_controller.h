//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

// Command Descriptor Block
using cdb_t = span<const int>;

class PrimaryDevice;

class AbstractController : public PhaseHandler
{

public:

    enum class shutdown_mode
    {
        none,
        stop_s2p,
        stop_pi,
        restart_pi
    };

    AbstractController(Bus&, int, int);
    ~AbstractController() override = default;

    virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::no_additional_sense_information,
        scsi_defs::status = scsi_defs::status::check_condition) = 0;

    virtual bool Process(int) = 0;

    virtual int GetEffectiveLun() const = 0;

    virtual void Reset();

    int GetInitiatorId() const
    {
        return initiator_id;
    }

    void ScheduleShutdown(shutdown_mode mode)
    {
        sh_mode = mode;
    }
    shutdown_mode GetShutdownMode() const
    {
        return sh_mode;
    }

    int GetTargetId() const
    {
        return target_id;
    }

    int GetLunCount() const
    {
        return static_cast<int>(luns.size());
    }

    unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
    shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
    bool AddDevice(shared_ptr<PrimaryDevice>);
    bool RemoveDevice(PrimaryDevice&);
    void ProcessOnController(int);

    void CopyToBuffer(const void*, size_t);
    auto& GetBuffer()
    {
        return buffer;
    }
    auto GetStatus() const
    {
        return status;
    }
    void SetStatus(scsi_defs::status s)
    {
        status = s;
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
    int GetCdbByte(int index) const
    {
        return cdb[index];
    }

    static constexpr int UNKNOWN_INITIATOR_ID = -1;

protected:

    void SetInitiatorId(int id)
    {
        initiator_id = id;
    }

    inline Bus& GetBus() const
    {
        return bus;
    }

    auto GetOpcode() const
    {
        return static_cast<scsi_defs::scsi_command>(cdb[0]);
    }

    int GetLun() const
    {
        return cdb[1] >> 5;
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

    int ExtractInitiatorId(int) const;

    array<int, 16> cdb = { };

    // Transfer data buffer, dynamically resized
    vector<uint8_t> buffer;
    // Transfer offset
    int offset = 0;
    // Total number of bytes to be transferred
    int total_length = 0;
    // Remaining bytes to be transferred in a single handshake cycle
    int current_length = 0;
    // The number of bytes to be transferred with the current handshake cycle
    int chunk_size = 0;

    scsi_defs::status status = status::good;

    Bus &bus;

    DeviceLogger device_logger;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    // The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
    int initiator_id = UNKNOWN_INITIATOR_ID;

    int max_luns;

    shutdown_mode sh_mode = shutdown_mode::none;
};
