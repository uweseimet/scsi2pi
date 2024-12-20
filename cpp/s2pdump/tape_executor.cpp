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

void TapeExecutor::SpaceBack()
{
    vector<uint8_t> cdb(6);
    cdb[1] = 0b000;
    SetInt24(cdb, 2, -1);

    if (initiator_executor->Execute(scsi_command::space_6, cdb, { }, 0, LONG_TIMEOUT, false)) {
        throw io_exception("Can't space back one block");
    }
}

int TapeExecutor::WriteFilemark()
{
    vector<uint8_t> cdb(6);
    SetInt24(cdb, 2, 1);

    return initiator_executor->Execute(scsi_command::write_filemarks_6, cdb, { }, 0, LONG_TIMEOUT, true);
}

int TapeExecutor::ReadWrite(span<uint8_t> buf, int length)
{
    vector<uint8_t> cdb(6);

    // Restore
    if (length) {
        SetInt24(cdb, 2, length);

        if (initiator_executor->Execute(scsi_command::write_6, cdb, buf, length, LONG_TIMEOUT, false)) {
            throw io_exception(fmt::format("Can't write block with {} byte(s)", length));
        }

        return length;
    }

    // Dump
    bool has_error = false;
    while (true) {
        SetInt24(cdb, 2, default_length);

        if (!initiator_executor->Execute(scsi_command::read_6, cdb, buf, default_length, LONG_TIMEOUT, false)) {
            GetLogger().debug("Read block with {} byte(s)", default_length);
            return default_length;
        }

        vector<uint8_t> sense_data(14);
        fill_n(cdb.begin(), cdb.size(), 0);
        cdb[4] = static_cast<uint8_t>(sense_data.size());
        const int status = initiator_executor->Execute(scsi_command::request_sense, cdb, sense_data, sense_data.size(),
            SHORT_TIMEOUT, false);
        if (status == 0xff) {
            return status;
        }
        else if (status && status != 0x02) {
            throw io_exception(fmt::format("Unknown error status {}", status));
        }

        // EOM?
        if (sense_data[2] & 0x40) {
            GetLogger().debug("No more data");
            return NO_MORE_DATA;
        }

        const sense_key sense_key = static_cast<enum sense_key>(sense_data[2] & 0x0f);

        if (sense_key == sense_key::medium_error) {
            if (has_error) {
                return BAD_BLOCK;
            }

            has_error = true;

            SpaceBack();

            continue;
        }

        if (sense_data[2] & 0x80) {
            GetLogger().debug("Encountered filemark");
            return 0;
        }

        // VALID and ILI?
        if (sense_data[0] & 0x80 && sense_data[2] & 0x20) {
            default_length -= GetInt32(sense_data, 3);

            // If all available data have been read there is no need to re-try
            if (default_length < length) {
                return default_length;
            }

            SpaceBack();
        }
        else {
            return 0xff;
        }
    }
}

void TapeExecutor::SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    buf[offset] = static_cast<uint8_t>(value >> 16);
    buf[offset + 1] = static_cast<uint8_t>(value >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}

