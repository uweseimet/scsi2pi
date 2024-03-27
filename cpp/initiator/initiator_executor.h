//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <ctime>
#include <stdexcept>
#include <memory>
#include <span>
#include "buses/bus.h"

using namespace std;

class InitiatorExecutor
{
    class phase_exception : public runtime_error
    {
        using runtime_error::runtime_error;
    };

public:

    InitiatorExecutor(Bus &b, int id) : bus(b), initiator_id(id)
    {
    }
    ~InitiatorExecutor() = default;

    void SetTarget(int, int, bool);

    // Execute command with a default timeout of 3 s
    int Execute(scsi_command, span<uint8_t>, span<uint8_t>, int, int = 3);

    int GetByteCount() const
    {
        return byte_count;
    }

private:

    bool Dispatch(scsi_command, span<uint8_t>, span<uint8_t>, int&);

    bool Arbitration() const;
    bool Selection() const;
    void Command(scsi_command, span<uint8_t>) const;
    void Status();
    void DataIn(span<uint8_t>, int&);
    void DataOut(span<uint8_t>, int&);
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

    int initiator_id;

    int target_id = -1;
    int target_lun = -1;

    int status = 0xff;

    int byte_count = 0;

    bool sasi = false;

    int next_message = 0x80;

    static constexpr timespec BUS_SETTLE_DELAY = { .tv_sec = 0, .tv_nsec = 400 };
    static constexpr timespec BUS_CLEAR_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec BUS_FREE_DELAY = { .tv_sec = 0, .tv_nsec = 800 };
    static constexpr timespec DESKEW_DELAY = { .tv_sec = 0, .tv_nsec = 45 };
    static constexpr timespec ARBITRATION_DELAY = { .tv_sec = 0, .tv_nsec = 2'400 };
};
