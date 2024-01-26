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
    vector < uint8_t > buf(14);
    array < uint8_t, 6 > cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());

    if (executor.Execute(scsi_command::cmd_request_sense, cdb, buf, static_cast<int>(buf.size()))) {
        return "Can't execute REQUEST SENSE";
    }

    if (executor.GetByteCount() < static_cast<int>(buf.size())) {
        return "Device reported an unknown error";
    }

    return FormatSenseData(static_cast<sense_key>(static_cast<byte>(buf[2]) & byte { 0x0f }), static_cast<asc>(buf[12]),
        buf[13]);
}

void initiator_util::LogStatus(int status)
{
    if (status) {
        if (const auto &it_status = STATUS_MAPPING.find(static_cast<scsi_defs::status>(status)); it_status
            != STATUS_MAPPING.end()) {
            warn("Device reported {0} (status code ${1:02x})", it_status->second, status);
        }
        else {
            warn("Device reported unknown status (status code ${0:02x})", status);
        }
    }
}

bool initiator_util::SetLogLevel(const string &log_level)
{
    const level::level_enum l = level::from_str(log_level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != log_level) {
        return false;
    }

    set_level(l);

    return true;
}
