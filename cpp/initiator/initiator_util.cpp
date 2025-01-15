//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "initiator_util.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

void initiator_util::ResetBus(Bus &bus)
{
    bus.SetRST(true);
    // 50 us should be enough, the specification requires at least 25 us
    const timespec ts = { .tv_sec = 0, .tv_nsec = 50'000 };
    nanosleep(&ts, nullptr);
    bus.Reset();
}

tuple<SenseKey, Asc, int> initiator_util::GetSenseData(InitiatorExecutor &executor)
{
    array<uint8_t, 255> buf = { };
    array<uint8_t, 6> cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());

    if (executor.Execute(ScsiCommand::REQUEST_SENSE, cdb, buf, static_cast<int>(buf.size()), 1, true)) {
        error("Can't execute REQUEST SENSE");
        return {SenseKey {-1}, Asc {-1}, -1};
    }

    trace(executor.FormatBytes(buf, executor.GetByteCount()));

    if (executor.GetByteCount() < 14) {
        warn("Device did not return standard REQUEST SENSE data, sense data details are not available");
        return {SenseKey {-1}, Asc {-1}, -1};
    }

    return {static_cast<SenseKey>(static_cast<int>(buf[2]) & 0x0f), static_cast<Asc>(buf[12]), buf[13]};
}

bool initiator_util::SetLogLevel(logger &logger, const string &log_level)
{
    // Default spdlog format without the timestamp
    logger.set_pattern("[%^%l%$] [%n] %v");

    if (log_level.empty()) {
        return true;
    }

    // Compensate for spdlog using 'off' for unknown levels
    if (const level::level_enum level = level::from_str(log_level); to_string_view(level) == log_level) {
        logger.set_level(level);
        return true;
    }

    return false;
}
