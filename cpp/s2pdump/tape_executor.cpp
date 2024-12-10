//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "tape_executor.h"
#include <spdlog/spdlog.h>
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;

int TapeExecutor::Rewind()
{
    vector<uint8_t> cdb(6);

    return initiator_executor->Execute(scsi_command::rewind, cdb, { }, 0, LONG_TIMEOUT, true);
}

int TapeExecutor::SpaceBack()
{
    vector<uint8_t> cdb(6);
    SetInt32(cdb, 1, -1);
    cdb[1] = 0b00;

    return initiator_executor->Execute(scsi_command::space_6, cdb, { }, 0, SHORT_TIMEOUT, true);
}

int TapeExecutor::WriteFilemark()
{
    vector<uint8_t> cdb(6);
    SetInt32(cdb, 1, 1);

    return initiator_executor->Execute(scsi_command::write_filemarks_6, cdb, { }, 0, SHORT_TIMEOUT, true);
}

int TapeExecutor::ReadWrite(span<uint8_t> buffer, int length)
{
    vector<uint8_t> cdb(6);

    bool retry = false;

    if (length) {
        cdb[2] = static_cast<uint8_t>(length >> 16);
        cdb[3] = static_cast<uint8_t>(length >> 8);
        cdb[4] = static_cast<uint8_t>(length);

        const int status = initiator_executor->Execute(scsi_command::write_6, cdb, buffer, length, LONG_TIMEOUT, false);
        if (status) {
            throw io_exception("Can't write to tape");
        }

        return length;
    }
    else {
        while (true) {
            cdb[2] = static_cast<uint8_t>(default_length >> 16);
            cdb[3] = static_cast<uint8_t>(default_length >> 8);
            cdb[4] = static_cast<uint8_t>(default_length);

            int status = initiator_executor->Execute(scsi_command::read_6, cdb, buffer, default_length, LONG_TIMEOUT,
                false);
            if (!status) {
                debug("Read {} byte(s) block", default_length);

                return default_length;
            }

            if (retry) {
                throw io_exception("Retry failed");
            }

            fill_n(cdb.begin(), cdb.size(), 0);
            cdb[4] = 14;
            status = initiator_executor->Execute(scsi_command::request_sense, cdb, buffer, 14, SHORT_TIMEOUT, false);
            if (status && status != 0x02) {
                throw io_exception("Unknown error");
            }

            if ((buffer[2] & 0x0f) == 0x08) {
                debug("No more data");
                return -1;
            }

            if (buffer[2] & 0x80) {
                debug("Hit filemark");
                return 0;
            }

            if (!(buffer[0] & 0x80)) {
                throw io_exception("VALID is not set");
            }

            default_length -= GetInt32(buffer, 3);

            if (SpaceBack()) {
                throw io_exception("Can't space back");
            }

            retry = true;
        }
    }

    return 0;
}
