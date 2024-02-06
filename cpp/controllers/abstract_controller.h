//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_set>
#include <span>
#include <vector>
#include "buses/bus.h"
#include "phase_handler.h"
#include "base/device_logger.h"

using namespace std;

class PrimaryDevice;

class AbstractController : public PhaseHandler
{

public:

    static inline const int UNKNOWN_INITIATOR_ID = -1;

    enum class shutdown_mode
    {
        NONE,
        STOP_S2P,
        STOP_PI,
        RESTART_PI
    };

    AbstractController(Bus&, int, int);
    ~AbstractController() override = default;

    virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::no_additional_sense_information,
        scsi_defs::status = scsi_defs::status::check_condition) = 0;
    virtual void Reset();
    virtual int GetInitiatorId() const = 0;

    virtual int GetEffectiveLun() const = 0;

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
    void SetCurrentLength(size_t);
    void SetTransferSize(uint32_t, uint32_t);
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
        uint32_t offset;
        // Total number of bytes to be transferred
        uint32_t total_length;
        // Remaining bytes to be transferred in a single handshake cycle
        uint32_t current_length;
        // The number of bytes to be transferred with a single handshake cycle
        uint32_t chunk_size;
    };

    ctrl_t ctrl = { };

    Bus &bus;

    DeviceLogger device_logger;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    int max_luns;

    shutdown_mode sh_mode = shutdown_mode::NONE;
};
