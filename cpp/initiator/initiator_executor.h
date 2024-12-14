//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <ctime>
#include <memory>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include "buses/bus.h"
#include "shared/s2p_formatter.h"

using namespace std;
using namespace spdlog;

class InitiatorExecutor
{
    class phase_exception : public runtime_error
    {
        using runtime_error::runtime_error;
    };

public:

    InitiatorExecutor(Bus &b, int id, logger &l) : bus(b), initiator_id(id), initiator_logger(l)
    {
    }
    ~InitiatorExecutor() = default;

    void SetTarget(int, int, bool);

    int Execute(scsi_command, span<uint8_t>, span<uint8_t>, int, int, bool);
    int Execute(span<uint8_t>, span<uint8_t>, int, int, bool);

    int GetByteCount() const
    {
        return byte_count;
    }

    logger& GetLogger()
    {
        return initiator_logger;
    }

    string FormatBytes(span<const uint8_t> bytes, int count) const
    {
        return formatter.FormatBytes(bytes, count);
    }

private:

    bool Dispatch(span<uint8_t>, span<uint8_t>, int&);

    bool Arbitration() const;
    bool Selection() const;
    void Command(span<uint8_t>);
    void Status();
    void DataIn(data_in_t, int&);
    void DataOut(data_out_t, int&);
    void MsgIn();
    void MsgOut();

    bool WaitForFree() const;
    bool WaitForBusy() const;

    void LogStatus() const;

    void Sleep(const timespec &ns) const
    {
        nanosleep(&ns, nullptr);
    }

    Bus &bus;

    const S2pFormatter formatter;

    int initiator_id;

    logger &initiator_logger;

    int target_id = -1;
    int target_lun = -1;

    int status = 0xff;

    int byte_count = 0;

    int cdb_offset = 0;

    bool sasi = false;

    message_code next_message = message_code::identify;

    static constexpr timespec BUS_SETTLE_DELAY = { .tv_sec = 0, .tv_nsec = 400 };
    static constexpr timespec BUS_CLEAR_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec BUS_FREE_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec DESKEW_DELAY = { .tv_sec = 0, .tv_nsec = 45 };
    static constexpr timespec ARBITRATION_DELAY = { .tv_sec = 0, .tv_nsec = 2'400 };
};
