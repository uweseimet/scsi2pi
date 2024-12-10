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

void TapeExecutor::Rewind()
{
    vector<uint8_t> cdb(6);

    initiator_executor->Execute(scsi_command::rewind, cdb, { }, 0, 300);
}

void TapeExecutor::Space(bool filemark, bool reverse)
{
    vector<uint8_t> cdb(6);
    cdb[1] = filemark ? 0b001 : 0b000;
    if (reverse) {
        cdb[2] = 0xff;
        cdb[3] = 0xff;
        cdb[4] = 0xff;
    }
    else {
        cdb[4] = 0x01;
    }

    initiator_executor->Execute(scsi_command::space_6, cdb, { }, 0, 3);
}

int TapeExecutor::ReadWrite(span<uint8_t> buffer, bool is_write)
{
    vector<uint8_t> cdb(6);
    SetInt32(cdb, 1, default_length);

    int status = initiator_executor->Execute(is_write ? scsi_command::write_6 : scsi_command::read_6, cdb, buffer,
        default_length, 300);
    if (!status) {
        return default_length;
    }

    fill_n(cdb.begin(), cdb.size(), 0);
    cdb[4] = 14;
    initiator_executor->Execute(scsi_command::request_sense, cdb, buffer, 14, 3);

    if (buffer[2] & 0x80) {
        debug("Hit filemark");
        return 0;
    }

    if (!(buffer[0] & 0x80)) {
        error("VALID is not set");
        return -1;
    }

    default_length -= GetInt32(buffer, 3);

    debug("Found block with {} byte(s)", default_length);

    SetInt32(cdb, 1, default_length);

    status = initiator_executor->Execute(is_write ? scsi_command::write_6 : scsi_command::read_6, cdb, buffer,
        default_length, 300);
    if (!status) {
        return default_length;
    }

    fill_n(cdb.begin(), cdb.size(), 0);
    cdb[4] = 14;
    initiator_executor->Execute(scsi_command::request_sense, cdb, buffer, 14, 3);
    spdlog::error(buffer[2] & 0x0f);
    spdlog::error(buffer[12]);
    // TODO Print sense data

    return -1;
}
