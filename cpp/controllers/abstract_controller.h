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

    AbstractController(Bus&, int, int);
    ~AbstractController() override = default;

    virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::no_additional_sense_information,
        scsi_defs::status = scsi_defs::status::check_condition) = 0;
    virtual void Reset();
    virtual int GetInitiatorId() const = 0;

    virtual int GetEffectiveLun() const = 0;

    enum class shutdown_mode
    {
        none,
        stop_s2p,
        stop_pi,
        restart_pi
    };

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
        return ctrl.buffer;
    }
    auto GetStatus() const
    {
        return ctrl.status;
    }
    void SetStatus(scsi_defs::status s)
    {
        ctrl.status = s;
    }
    auto GetChunkSize() const
    {
        return ctrl.chunk_size;
    }
    auto GetCurrentLength() const
    {
        return ctrl.current_length;
    }
    void SetCurrentLength(int);
    void SetTransferSize(int, int);
    void SetMessage(int m)
    {
        ctrl.message = m;
    }
    auto& GetCdb() const
    {
        return ctrl.cdb;
    }
    int GetCdbByte(int index) const
    {
        return ctrl.cdb[index];
    }

    inline static const int UNKNOWN_INITIATOR_ID = -1;

protected:

    inline Bus& GetBus() const
    {
        return bus;
    }

    auto GetOpcode() const
    {
        return static_cast<scsi_defs::scsi_command>(ctrl.cdb[0]);
    }
    int GetLun() const
    {
        return (ctrl.cdb[1] >> 5) & 0x07;
    }

    void SetCdbByte(int index, int value)
    {
        ctrl.cdb[index] = value;
    }

    bool UpdateTransferSize()
    {
        ctrl.total_length -= ctrl.chunk_size;
        return ctrl.total_length != 0;
    }
    auto GetMessage() const
    {
        return ctrl.message;
    }

    auto GetOffset() const
    {
        return ctrl.offset;
    }
    void ResetOffset()
    {
        ctrl.offset = 0;
    }
    void UpdateOffsetAndLength()
    {
        ctrl.offset += ctrl.current_length;
        ctrl.current_length = 0;
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

    using ctrl_t = struct _ctrl_t {
        // Command data
        array<int, 16> cdb;

        // Status data
        scsi_defs::status status;
        // Message data
        int message;

        // Transfer data buffer, dynamically resized
        vector<uint8_t> buffer;
        // Transfer offset
        int offset;
        // Total number of bytes to be transferred
        int total_length;
        // Remaining bytes to be transferred in a single handshake cycle
        int current_length;
        // The number of bytes to be transferred with a single handshake cycle
        int chunk_size;
    };

    ctrl_t ctrl = { };

    Bus &bus;

    DeviceLogger device_logger;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    int max_luns;

    shutdown_mode sh_mode = shutdown_mode::none;
};
