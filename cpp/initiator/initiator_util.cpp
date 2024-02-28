//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "shared/s2p_util.h"
#include "initiator_util.h"

using namespace spdlog;
using namespace scsi_defs;
using namespace s2p_util;

void initiator_util::ResetBus(Bus &bus)
{
    bus.SetRST(true);
    // 100 us should be enough, the standard requires at least 25 us
    const timespec ts = { .tv_sec = 0, .tv_nsec = 100'000 };
    nanosleep(&ts, nullptr);
    bus.Reset();
}

tuple<sense_key, asc, int> initiator_util::GetSenseData(InitiatorExecutor &executor)
{
    array<uint8_t, 14> buf = { };
    array<uint8_t, 6> cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());

    if (executor.Execute(scsi_command::cmd_request_sense, cdb, buf, static_cast<int>(buf.size()))) {
        error("Can't execute REQUEST SENSE");
        return {sense_key {-1}, asc {-1}, -1};
    }

    if (executor.GetByteCount() < static_cast<int>(buf.size())) {
        warn("Device did not return standard REQUEST SENSE data");
        return {sense_key {-1}, asc {-1}, -1};
    }

    return {static_cast<sense_key>(buf[2] & 0x0f), static_cast<asc>(buf[12]), buf[13]}; // NOSONAR Using byte type does not work with the bullseye compiler
}

bool initiator_util::SetLogLevel(const string &log_level)
{
    // Default spdlog format without the date
    set_pattern("[%T.%e] [%^%l%$] %v");

    // Compensate for spdlog using 'off' for unknown levels
    if (const level::level_enum level = level::from_str(log_level); to_string_view(level) == log_level) {
        set_level(level);
        return true;
    }

    return false;
}
