//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Base class for device controllers
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
    int GetMaxLuns() const
    {
        return max_luns;
    }
    int GetLunCount() const
    {
        return static_cast<int>(luns.size());
    }

    unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
    shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
    bool AddDevice(shared_ptr<PrimaryDevice>);
    bool RemoveDevice(PrimaryDevice&);
    bool HasDeviceForLun(int) const;
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
    auto GetLength() const
    {
        return ctrl.length;
    }
    void SetLength(size_t);
    void SetBlocks(uint32_t b)
    {
        ctrl.blocks = b;
    }
    void SetNext(uint64_t n)
    {
        ctrl.next = n;
    }
    void SetMessage(int m)
    {
        ctrl.message = m;
    }
    auto GetCmd() const
    {
        return ctrl.cmd;
    }
    int GetCmdByte(int index) const
    {
        return ctrl.cmd[index];
    }
    void SetByteTransfer(bool);

protected:

    inline Bus& GetBus() const
    {
        return bus;
    }

    auto GetOpcode() const
    {
        return static_cast<scsi_defs::scsi_command>(ctrl.cmd[0]);
    }
    int GetLun() const
    {
        return (ctrl.cmd[1] >> 5) & 0x07;
    }

    void AllocateCmd(size_t);

    void SetCmdByte(int index, int value)
    {
        ctrl.cmd[index] = value;
    }

    bool HasBlocks() const
    {
        return ctrl.blocks;
    }
    void DecrementBlocks()
    {
        --ctrl.blocks;
    }
    auto GetNext() const
    {
        return ctrl.next;
    }
    void IncrementNext()
    {
        ++ctrl.next;
    }
    int GetMessage() const
    {
        return ctrl.message;
    }

    bool HasValidLength() const
    {
        return ctrl.length != 0;
    }
    int GetOffset() const
    {
        return ctrl.offset;
    }
    void ResetOffset()
    {
        ctrl.offset = 0;
    }
    void UpdateOffsetAndLength()
    {
        ctrl.offset += ctrl.length;
        ctrl.length = 0;
    }

    bool IsByteTransfer() const
    {
        return is_byte_transfer;
    }
    void InitBytesToTransfer()
    {
        bytes_to_transfer = ctrl.length;
    }
    auto GetBytesToTransfer() const
    {
        return bytes_to_transfer;
    }

    void LogTrace(const string &s) const
    {
        device_logger.Trace(s);
    }
    void LogDebug(const string &s) const
    {
        device_logger.Debug(s);
    }
    void LogInfo(const string &s) const
    {
        device_logger.Info(s);
    }
    void LogWarn(const string &s) const
    {
        device_logger.Warn(s);
    }
    void LogError(const string &s) const
    {
        device_logger.Error(s);
    }

private:

    int ExtractInitiatorId(int) const;

    using ctrl_t = struct _ctrl_t {
        // Command data, dynamically resized if required
        vector<int> cmd = vector<int>(16);

        // Status data
        scsi_defs::status status;
        // Message data
        int message;

        // Transfer data buffer, dynamically resized if required
        vector<uint8_t> buffer;
        // Number of transfer blocks
        uint32_t blocks;
        // Next record
        uint64_t next;
        // Transfer offset
        uint32_t offset;
        // Remaining bytes to be transferred
        uint32_t length;
    };

    ctrl_t ctrl = { };

    Bus &bus;

    DeviceLogger device_logger;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    int max_luns;

    bool is_byte_transfer = false;
    uint32_t bytes_to_transfer = 0;

    shutdown_mode sh_mode = shutdown_mode::NONE;
};
