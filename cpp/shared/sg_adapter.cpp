//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_adapter.h"
#include <iostream>
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/sg_util.h"

using namespace memory_util;
using namespace sg_util;

string SgAdapter::Init(const string &d)
{
    try {
        fd = OpenDevice(d);
    }
    catch (const IoException &e) {
        return e.what();
    }

    device = d;

    GetBlockSize();

    return "";
}

void SgAdapter::CleanUp()
{
    if (fd != -1) {
        close (fd);
        fd = -1;
    }
}

int SgAdapter::SendCommand(span<const uint8_t> cdb, span<uint8_t> buf, int total_length, int timeout)
{
    byte_count = 0;

    const int allocation_length = GetAllocationLength(cdb);
    total_length = allocation_length ? allocation_length : total_length;

    vector<uint8_t> local_cdb = { cdb.begin(), cdb.end() };

    int offset = 0;
    do {
        const int length = total_length < MAX_TRANSFER_LENGTH ? total_length : MAX_TRANSFER_LENGTH;
        SetBlockCount(local_cdb, length / block_size);

        if (const int status = SendCommandInternal(local_cdb, span(buf.data() + offset, buf.size() - offset), length,
            timeout, true); status
            || !command_meta_data.GetCdbMetaData(static_cast<ScsiCommand>(cdb[0])).block_size) {
            return status;
        }

        offset += length;
        total_length -= length;

        UpdateStartBlock(local_cdb, length / block_size);
    } while (total_length);

    return 0;
}

int SgAdapter::SendCommandInternal(span<uint8_t> cdb, span<uint8_t> buf, int length, int timeout,
    bool enable_log)
{
    // Return deferred sense data, if any
    if (cdb[0] == static_cast<uint8_t>(ScsiCommand::REQUEST_SENSE) && sense_data_valid) {
        const int l = min(length, static_cast<int>(sense_data.size()));
        memcpy(buf.data(), sense_data.data(), l);
        byte_count = l;
        sense_data_valid = false;
        return 0;
    }
    sense_data_valid = false;

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (buf.empty()) {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
    else {
        io_hdr.dxfer_direction =
            command_meta_data.GetCdbMetaData(static_cast<ScsiCommand>(cdb[0])).has_data_out ?
                SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf.data() : nullptr;

    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = static_cast<uint8_t>(sense_data.size());

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    if (enable_log && sg_logger.level() <= level::debug) {
        sg_logger.debug(command_meta_data.LogCdb(cdb, fmt::format("SG driver ({})", device)));
    }

    const int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;
    if (!EvaluateStatus(status, buf, cdb)) {
        return status;
    }

    byte_count += length - io_hdr.resid;

    return status;
}

bool SgAdapter::EvaluateStatus(int status, span<uint8_t> buf, span<uint8_t> cdb)
{
    if (status == -1) {
        return false;
    }
    // Do not consider CONDITION MET an error
    else if (status == static_cast<int>(StatusCode::CONDITION_MET)) {
        status = static_cast<int>(StatusCode::GOOD);
    }

    if (!status && cdb[0] == static_cast<uint8_t>(ScsiCommand::INQUIRY) && (static_cast<int>(cdb[1]) & 0b11100000)) {
        // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
        buf[0] = 0x7f;
    }

    if (status) {
        sense_data_valid = true;
    }

    return true;
}

void SgAdapter::GetBlockSize()
{
    vector<uint8_t> buf(8);
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CAPACITY_10);

    try {
        if (!SendCommandInternal(cdb, buf, static_cast<int>(buf.size()), 1, false)) {
            block_size = GetInt32(buf, 4);
            assert(block_size);
        }
    }
    catch (const ScsiException&) { // NOSONAR The exception details do not matter
        // Fall through
    }
}
