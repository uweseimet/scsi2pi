//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_adapter.h"
#include <array>
#include <iostream>
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <spdlog/spdlog.h>
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/sg_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace sg_util;

string SgAdapter::Init(const string &device)
{
    try {
        fd = OpenDevice(device);
    }
    catch (const io_exception &e) {
        return e.what();
    }

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

SgAdapter::SgResult SgAdapter::SendCommand(span<uint8_t> cdb, span<uint8_t> buf, int total_length, int timeout)
{
    byte_count = 0;

    const int allocation_length = GetAllocationLength(cdb);
    total_length = allocation_length ? allocation_length : total_length;

    vector<uint8_t> local_cdb = { cdb.begin(), cdb.end() };

    int offset = 0;
    do {
        const int length = total_length < MAX_TRANSFER_LENGTH ? total_length : MAX_TRANSFER_LENGTH;
        SetBlockCount(local_cdb, length / block_size);

        if (const auto &result = SendCommandInternal(local_cdb, span(buf.data() + offset, buf.size() - offset), length,
            timeout, true); result.status
            || !command_meta_data.GetCdbMetaData(static_cast<scsi_command>(cdb[0])).block_size) {
            return {result.status, byte_count};
        }

        offset += length;
        total_length -= length;

        UpdateStartBlock(local_cdb, length / block_size);
    } while (total_length);

    return {0, byte_count};
}

SgAdapter::SgResult SgAdapter::SendCommandInternal(span<uint8_t> cdb, span<uint8_t> buf, int length, int timeout,
    bool log)
{
    // Return deferred sense data, if any
    if (cdb[0] == static_cast<uint8_t>(scsi_command::request_sense) && sense_data_valid) {
        const int l = min(length, static_cast<int>(sense_data.size()));
        memcpy(buf.data(), sense_data.data(), l);
        byte_count = l;
        sense_data_valid = false;
        return {0, 0};
    }
    sense_data_valid = false;

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (buf.empty()) {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
    else {
        io_hdr.dxfer_direction =
            command_meta_data.GetCdbMetaData(static_cast<scsi_command>(cdb[0])).has_data_out ?
                SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf.data() : nullptr;

    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = static_cast<uint8_t>(sense_data.size());

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    if (log && sg_logger.level() <= level::debug) {
        sg_logger.debug(command_meta_data.LogCdb(cdb, "SG driver"));
    }

    int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;
    if (status == -1) {
        return {status, length};
    }
    // Do not consider CONDITION MET an error
    else if (status == static_cast<int>(status_code::condition_met)) {
        status = static_cast<int>(status_code::good);
    }

    // If the command was successful, use the sense key as status
    if (!status) {
        status = static_cast<int>(sense_data[2]) & 0x0f;

        if (cdb[0] == static_cast<uint8_t>(scsi_command::inquiry) && (static_cast<int>(cdb[1]) & 0b11100000)) {
            // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
            buf[0] = 0x7f;
        }
    }

    if (status) {
        sense_data_valid = true;
    }

    byte_count += length - io_hdr.resid;

    return {status, length - io_hdr.resid};
}

void SgAdapter::GetBlockSize()
{
    vector<uint8_t> buf(8);
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(scsi_command::read_capacity_10);

    try {
        if (!SendCommandInternal(cdb, buf, static_cast<int>(buf.size()), 1, false).status) {
            block_size = GetInt32(buf, 4);
            assert(block_size);
        }
    }
    catch (const scsi_exception&) { // NOSONAR The exception details do not matter
        // Fall through
    }
}
