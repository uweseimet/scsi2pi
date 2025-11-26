//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <spdlog/spdlog.h>
#include "shared/s2p_defs.h"
#include "shared/s2p_formatter.h"
#include "shared/scsi.h"

class Bus;

using namespace std;
using namespace spdlog;

class InitiatorExecutor
{
    class PhaseException : public runtime_error
    {
        using runtime_error::runtime_error;
    };

public:

    InitiatorExecutor(Bus &b, int id, logger &l) : bus(b), initiator_id(id), initiator_logger(l)
    {
    }
    ~InitiatorExecutor() = default;

    void SetTarget(int, int, bool);

    int Execute(span<uint8_t>, span<uint8_t>, int, int, bool);

    tuple<SenseKey, Asc, int> GetSenseData();

    void ResetBus();

    int GetByteCount() const
    {
        return byte_count;
    }

    void SetLimit(int limit)
    {
        formatter.SetLimit(limit);
    }

private:

    bool Dispatch(span<uint8_t>, span<uint8_t>, int&);

    bool Arbitration() const;
    bool Selection(bool) const;
    void Command(span<uint8_t>);
    void Status();
    void DataIn(data_in_t, int&);
    void DataOut(data_out_t);
    void MsgIn();
    void MsgOut();

    bool WaitForFree() const;
    bool WaitForBusy() const;

    void Sleep(const timespec &ns) const
    {
        nanosleep(&ns, nullptr);
    }

    Bus &bus;

    S2pFormatter formatter;

    int initiator_id;

    logger &initiator_logger;

    int target_id = -1;
    int target_lun = -1;

    int status_code = 0xff;

    int byte_count = 0;

    int cdb_offset = 0;

    bool sasi = false;

    MessageCode next_message = MessageCode::IDENTIFY;

    static constexpr timespec BUS_SETTLE_DELAY = { .tv_sec = 0, .tv_nsec = 400 };
    static constexpr timespec BUS_CLEAR_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec BUS_FREE_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec DESKEW_DELAY = { .tv_sec = 0, .tv_nsec = 45 };
    static constexpr timespec ARBITRATION_DELAY = { .tv_sec = 0, .tv_nsec = 2'400 };
};
