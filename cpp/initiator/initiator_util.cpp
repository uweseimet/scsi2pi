//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "initiator_util.h"

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
