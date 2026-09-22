//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_context.h"
#include <iostream>
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_util.h"

using namespace s2p_util;
using namespace protobuf_util;

bool CommandContext::ReadCommand()
{
    // Read magic string
    if (array<byte, 6> magic; ReadBytes(fd, magic)) {
        if (memcmp(magic.data(), "RASCSI", magic.size())) {
            throw IoException("Invalid magic");
        }

        // Fetch the command
        DeserializeMessage(fd, command);

        return true;
    }

    return false;
}

bool CommandContext::WriteResult(const PbResult &result) const
{
    // The descriptor is -1 when devices are not attached via the remote interface but by s2p
    if (fd != -1) {
        SerializeMessage(fd, result);
    }

    return result.status();
}

bool CommandContext::WriteSuccessResult(PbResult &result) const
{
    result.set_status(true);
    return WriteResult(result);
}

bool CommandContext::ReturnStatus(bool status, const string &msg, PbErrorCode error_code, bool enable_log) const
{
    // Do not log twice if logging has already been done in the localized error handling above
    if (enable_log && !status && !msg.empty()) {
        context_logger.error(msg);
    }

    if (fd == -1) {
        if (!msg.empty()) {
            cerr << "Error: " << msg << '\n';
        }
        return status;
    }

    PbResult result;
    result.set_status(status);
    result.set_error_code(error_code);
    result.set_msg(msg);
    return WriteResult(result);
}

bool CommandContext::ReturnSuccessStatus() const
{
    return ReturnStatus(true, "", PbErrorCode::NO_ERROR_CODE, true);
}

bool CommandContext::ReturnErrorStatus(const string &msg) const
{
    return ReturnStatus(false, msg, PbErrorCode::NO_ERROR_CODE, true);
}
