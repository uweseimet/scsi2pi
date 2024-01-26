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

string initiator_util::GetSenseData(InitiatorExecutor &executor)
{
    array<uint8_t, 14> buf;
    array<uint8_t, 6> cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());

    if (executor.Execute(scsi_command::cmd_request_sense, cdb, buf, static_cast<int>(buf.size()))) {
        return "Can't execute REQUEST SENSE";
    }

    if (executor.GetByteCount() < static_cast<int>(buf.size())) {
        return "Device did not return standard REQUEST SENSE data";
    }

    return FormatSenseData(static_cast<sense_key>(buf[2] & 0x0f), static_cast<asc>(buf[12]), buf[13]); // NOSONAR Using byte causes an issue with the bullseye compiler
}

bool initiator_util::SetLogLevel(const string &log_level)
{
    // Compensate for spdlog using 'off' for unknown levels
    if (const level::level_enum level = level::from_str(log_level); to_string_view(level) == log_level) {
        set_level(level);
        return true;
    }

    return false;
}
