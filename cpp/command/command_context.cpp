//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
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
    array<byte, 6> magic;
    if (const auto bytes_read = ReadBytes(fd, magic); bytes_read) {
        if (bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
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

bool CommandContext::ReturnLocalizedError(LocalizationKey key, const string &arg1, const string &arg2,
    const string &arg3) const
{
    return ReturnLocalizedError(key, NO_ERROR_CODE, arg1, arg2, arg3);
}

bool CommandContext::ReturnLocalizedError(LocalizationKey key, PbErrorCode error_code, const string &arg1,
    const string &arg2, const string &arg3) const
{
    static const CommandLocalizer command_localizer;

    // For the logfile always use English
    // Do not log unknown operations as an error for backward/forward compatibility with old/new clients
    if (error_code == PbErrorCode::UNKNOWN_OPERATION) {
        s2p_logger.trace(command_localizer.Localize(key, "en", arg1, arg2, arg3));
    }
    else {
        s2p_logger.error(command_localizer.Localize(key, "en", arg1, arg2, arg3));
    }

    return ReturnStatus(false, command_localizer.Localize(key, locale, arg1, arg2, arg3), error_code, false);
}

bool CommandContext::ReturnStatus(bool status, const string &msg, PbErrorCode error_code, bool enable_log) const
{
    // Do not log twice if logging has already been done in the localized error handling above
    if (enable_log && !status && !msg.empty()) {
        s2p_logger.error(msg);
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
