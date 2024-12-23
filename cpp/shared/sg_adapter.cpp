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
#include "shared/command_meta_data.h"
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/sg_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace sg_util;

string SgAdapter::Init(const string &device)
{
    if (!device.starts_with("/dev/sg")) {
        return fmt::format("Missing or invalid device file: '{}'", device);
    }

    fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        return fmt::format("Can't open '{0}': {1}", device, strerror(errno));
    }

    if (int v; ioctl(fd, SG_GET_VERSION_NUM, &v) < 0 || v < 30000) {
        CleanUp();
        return fmt::format("'{0}' is not supported by the Linux SG 3 driver: {1}", device, strerror(errno));
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
    vector<uint8_t> local_cdb = { cdb.begin(), cdb.end() };

    int offset = 0;
    do {
        const int length = total_length < MAX_TRANSFER_LENGTH ? total_length : MAX_TRANSFER_LENGTH;
        SetBlockCount(local_cdb, length / block_size);

        if (const auto &result = SendCommandInternal(local_cdb, span(buf.data() + offset, buf.size() - offset), length,
            timeout); result.status) {
            return result;
        }

        offset += length;
        total_length -= length;

        UpdateStartBlock(local_cdb, length / block_size);
    } while (total_length);

    return {0, total_length};
}

SgAdapter::SgResult SgAdapter::SendCommandInternal(span<uint8_t> cdb, span<uint8_t> buf, int length, int timeout)
{
    // Return deferred sense data, if any
    if (cdb[0] == static_cast<uint8_t>(scsi_command::request_sense) && sense_data_valid) {
        const int l = min(length, static_cast<int>(sense_data.size()));
        memcpy(buf.data(), sense_data.data(), l);
        sense_data_valid = false;
        return {0, length - l};
    }
    sense_data_valid = false;

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (buf.empty()) {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
    else {
        io_hdr.dxfer_direction =
            CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])).has_data_out ?
                SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf.data() : nullptr;

    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = static_cast<uint8_t>(sense_data.size());

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    sg_logger.debug(CommandMetaData::Instance().LogCdb(cdb, "SG driver"));

    int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;
    if (status == -1) {
        return {status, length};
    }
    // Do not treat CONDITION MET as an error
    if (status == 4) {
        status = 0;
    }

    // If the command was successful, use the sense key as status
    if (!status) {
        status = static_cast<int>(sense_data[2]) & 0x0f;

        if (cdb[0] == static_cast<uint8_t>(scsi_command::inquiry) && (static_cast<int>(cdb[1]) & 0b11100000)) {
            // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
            buf[0] = 0x7f;
        }
    }

    sense_data_valid = true;

    return {status, length - io_hdr.resid};
}

void SgAdapter::GetBlockSize()
{
    vector<uint8_t> buf(8);
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(scsi_command::read_capacity_10);

    try {
        if (!SendCommandInternal(cdb, buf, static_cast<int>(buf.size()), 1).status) {
            block_size = GetInt32(buf, 4);
            assert(block_size);
        }
    }
    catch (const scsi_exception&) { // NOSONAR The exception details do not matter
        // Fall through
    }
}
