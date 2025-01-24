//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <span>
#include <unordered_set>
#include "phase_handler.h"
#include "shared/s2p_defs.h"
#include "shared/s2p_formatter.h"

class Bus;
class PrimaryDevice;
class ScriptGenerator;
namespace spdlog
{
class logger;
}

using namespace spdlog;

class AbstractController : public PhaseHandler
{

public:

    AbstractController(Bus&, int, const S2pFormatter&);
    ~AbstractController() override = default;

    virtual void Error(SenseKey, Asc, StatusCode) = 0;

    virtual int GetEffectiveLun() const = 0;

    virtual void Reset();

    void CleanUp() const;

    void SetScriptGenerator(shared_ptr<ScriptGenerator>);

    int GetInitiatorId() const
    {
        return initiator_id;
    }

    void ScheduleShutdown(ShutdownMode mode)
    {
        shutdown_mode = mode;
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
    ShutdownMode ProcessOnController(int);

    void CopyToBuffer(const void*, size_t);
    auto& GetBuffer() const
    {
        return buffer;
    }
    auto GetStatus() const
    {
        return status;
    }
    void SetStatus(StatusCode s)
    {
        status = s;
    }
    auto GetChunkSize() const
    {
        return chunk_size;
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

    string FormatBytes(span<const uint8_t> buf, size_t count) const
    {
        return formatter.FormatBytes(buf, count);
    }

    logger& GetLogger() const
    {
        return *controller_logger;
    }

protected:

    void AddCdbToScript();
    void AddDataToScript(span<const uint8_t>) const;

    virtual bool Process() = 0;

    Bus& GetBus() const
    {
        return bus;
    }

    void SetCdbByte(int index, int value)
    {
        cdb[index] = value;
    }

    auto GetOffset() const
    {
        return offset;
    }
    void ResetOffset()
    {
        offset = 0;
    }

    void UpdateTransferLength(int);
    void UpdateOffsetAndLength();

    void LogTrace(const string&) const;
    void LogDebug(const string&) const;
    void LogWarn(const string&) const;

private:

    array<int, 16> cdb = { };

    // Shared transfer data buffer, dynamically resized
    inline static auto buffer = vector<uint8_t>(512);
    // Transfer offset
    int offset = 0;
    // Total remaining bytes to be transferred, updated during the transfer
    int remaining_length = 0;
    // Remaining bytes to be transferred in a single handshake cycle
    int current_length = 0;
    // The number of bytes to be transferred with the current handshake cycle
    int chunk_size = 0;

    StatusCode status = StatusCode::GOOD;

    Bus &bus;

    shared_ptr<logger> controller_logger;

    shared_ptr<ScriptGenerator> script_generator;

    // Logical units of this controller mapped to their LUN numbers
    unordered_map<int, shared_ptr<PrimaryDevice>> luns;

    int target_id;

    const S2pFormatter &formatter;

    static constexpr int UNKNOWN_INITIATOR_ID = -1;

    // The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
    int initiator_id = UNKNOWN_INITIATOR_ID;

    ShutdownMode shutdown_mode = ShutdownMode::NONE;
};
