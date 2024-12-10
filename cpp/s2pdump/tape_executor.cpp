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

using namespace spdlog;
using namespace memory_util;

int TapeExecutor::Rewind()
{
    vector<uint8_t> cdb(6);

    return initiator_executor->Execute(scsi_command::rewind, cdb, { }, 0, 300, true);
}

int TapeExecutor::Space(bool filemark, int count)
{
    vector<uint8_t> cdb(6);
    SetInt32(cdb, 1, count);
    cdb[1] = filemark ? 0b001 : 0b000;

    return initiator_executor->Execute(scsi_command::space_6, cdb, { }, 0, 3, true);
}

int TapeExecutor::WriteFilemark()
{
    vector<uint8_t> cdb(6);
    SetInt32(cdb, 1, 1);

    return initiator_executor->Execute(scsi_command::write_filemarks_6, cdb, { }, 0, 3, true);
}

int TapeExecutor::ReadWrite(span<uint8_t> buffer, int length)
{
    vector<uint8_t> cdb(6);

    bool retry = false;

    if (length) {
        cdb[2] = static_cast<uint8_t>(length >> 16);
        cdb[3] = static_cast<uint8_t>(length >> 8);
        cdb[4] = static_cast<uint8_t>(length);

        const int status = initiator_executor->Execute(scsi_command::write_6, cdb, buffer, length, 300, false);
        if (!status) {
            return length;
        }

        error("Error");
        return 0;
    }
    else {
        while (true) {
            cdb[2] = static_cast<uint8_t>(default_length >> 16);
            cdb[3] = static_cast<uint8_t>(default_length >> 8);
            cdb[4] = static_cast<uint8_t>(default_length);

            int status = initiator_executor->Execute(scsi_command::read_6, cdb, buffer, default_length, 300, false);
            if (!status) {
                debug("Read 80 {} byte(s) block", default_length);

                return default_length;
            }

            if (retry) {
                error("Retry failed");
                return -2;
            }

            fill_n(cdb.begin(), cdb.size(), 0);
            cdb[4] = 14;
            status = initiator_executor->Execute(scsi_command::request_sense, cdb, buffer, 14, 3, false);
            if (status && status != 0x02) {
                error("Unknown error");
                return -2;
            }

            if ((buffer[2] & 0x0f) == 0x08) {
                debug("No more data");
                return -2;
            }

            if (buffer[2] & 0x80) {
                debug("Hit filemark");
                return 0;
            }

            if (!(buffer[0] & 0x80)) {
                error("VALID is not set");
                return -1;
            }

            default_length -= GetInt32(buffer, 3);

            // Space back
            if (Space(false, -1)) {
                error("Can't space back");
                return -2;
            }

            retry = true;
        }
    }

    return -1;
}
