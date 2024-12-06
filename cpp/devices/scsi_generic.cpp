//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Implementation of a generic SCSI device, using the Linux SG driver
//
//---------------------------------------------------------------------------

// TODO Convert READ/WRITE(6) to READ/WRITE(10) if required, otherwise old drivers may not work
// TODO MODE SELECT?

#ifdef __linux__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"
#include "scsi_generic.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

ScsiGeneric::ScsiGeneric(int lun) : PrimaryDevice(SCSG, scsi_level::scsi_2, lun)
{
    SupportsParams(true);
    SetReady(true);
}

bool ScsiGeneric::SetUp()
{
    device = GetParam(DEVICE);
    if (!device.starts_with("/dev/sg")) {
        LogError(fmt::format("Missing or invalid device file parameter"));
        return false;
    }

    fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        LogError(fmt::format("Can't open '{0}': {1}", device, strerror(errno)));
        return false;
    }

    return true;
}

void ScsiGeneric::CleanUp()
{
    if (fd != -1) {
        close(fd);
    }
}

void ScsiGeneric::Dispatch(scsi_command cmd)
{
    count = CommandMetaData::Instance().GetCommandBytesCount(cmd);
    assert(count);

    local_cdb.clear();
    for (int i = 0; i < count; i++) {
        local_cdb.push_back(static_cast<uint8_t>(GetController()->GetCdb()[i]));
    }

    if (const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(cmd); meta_data.block_size)
    {
        byte_count = GetAllocationLength() * block_size;
    }
    else {
        byte_count = GetAllocationLength();
    }

    // FORMAT UNIT is special because the parameter list length can be part of the data sent with DATA OUT
    if (local_cdb[0] == static_cast<uint8_t>(scsi_command::format_unit) && (static_cast<int>(local_cdb[1])
        & 0x10)) {
        // There must at least be the format list header, which has to be evaluated at the beginning of DATA OUT
        byte_count = 4;
    }

    remaining_count = byte_count;

    auto &buf = GetController()->GetBuffer();

    // There is no explicit LUN support, the SG driver maps each LUN to a device file
    if (GetController()->GetEffectiveLun() && cmd != scsi_command::inquiry) {
        if (cmd != scsi_command::request_sense) {
            throw scsi_exception(sense_key::illegal_request, asc::logical_unit_not_supported);
        }

        fill_n(buf.begin(), 18, 0);
        buf[0] = 0x70;
        buf[2] = static_cast<uint8_t>(sense_key::illegal_request);
        buf[7] = 10;
        buf[12] = static_cast<uint8_t>(asc::logical_unit_not_supported);

        const int length = min(18, byte_count);
        GetController()->SetTransferSize(length, length);

        DataInPhase(length);

        // When signalling an invalid LUN, for REQUEST SENSE the status must be GOOD
        return;
    }

    if (cmd == scsi_command::request_sense && deferred_sense_data_valid) {
        memcpy(buf.data(), deferred_sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = false;

        const int length = min(18, byte_count);
        GetController()->SetTransferSize(length, length);

        DataInPhase(length);

        // REQUEST SENSE does not fail
        return;
    }

    deferred_sense_data_valid = false;

    LogTrace(fmt::format("Preparing transfer of {} byte(s) with SG driver", byte_count));

    const int chunk_size = byte_count < MAX_TRANSFER_LENGTH ? byte_count : MAX_TRANSFER_LENGTH;

    // Split the transfer into chunks of MAX_TRANFER_LENGTH bytes
    GetController()->SetTransferSize(byte_count, chunk_size);

    if (WRITE_COMMANDS.contains(cmd)) {
        DataOutPhase(chunk_size);
    }
    else {
        GetController()->SetCurrentLength(byte_count);
        DataInPhase (ReadData(buf));
    }
}

param_map ScsiGeneric::GetDefaultParams() const
{
    return {
        {   DEVICE, ""}
    };
}

vector<uint8_t> ScsiGeneric::InquiryInternal() const
{
    assert(false);
    return {};
}

int ScsiGeneric::ReadData(data_in_t buf)
{
    return ReadWriteData(buf, false, GetController()->GetChunkSize());
}

void ScsiGeneric::WriteData(cdb_t, data_out_t buf, int, int chunk_size)
{
    // Evaluate the FORMAT UNIT format list header with the first chunk, and update the transfer size
    if (static_cast<scsi_command>(local_cdb[0]) == scsi_command::format_unit
        && (static_cast<int>(local_cdb[1]) & 0x10)) {
        if (format_header.empty()) {
            format_header.push_back(buf[0]);
            format_header.push_back(buf[1]);
            format_header.push_back(buf[2]);
            format_header.push_back(buf[3]);
            byte_count = GetInt16(buf, 2);
            remaining_count = byte_count;
            GetController()->SetTransferSize(byte_count, byte_count);

            // TODO Return pending data length?
            return;
        }
        else {
            remaining_count = byte_count;
            for (int i = 0; i < remaining_count; i++) {
                format_header.push_back(buf[i]);
            }
            buf = format_header;
        }
    }

    ReadWriteData(span((uint8_t*)buf.data(), buf.size()), true, chunk_size); // NOSONAR Cast required for SG driver API
}

int ScsiGeneric::ReadWriteData(span<uint8_t> buf, bool write, int chunk_size)
{
    int length = remaining_count < chunk_size ? remaining_count : chunk_size;
    length = length < MAX_TRANSFER_LENGTH ? length : MAX_TRANSFER_LENGTH;
    SetBlockCount(length / block_size);

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (length) {
        io_hdr.dxfer_direction = write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }
    else {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf.data() : nullptr;

    array<uint8_t, 18> sense_data = { };
    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = sense_data.size();

    io_hdr.cmdp = local_cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(local_cdb.size());

    io_hdr.timeout = (
        local_cdb[0] == static_cast<uint8_t>(scsi_command::format_unit) ?
            TIMEOUT_FORMAT_SECONDS : TIMEOUT_DEFAULT_SECONDS) * 1000;

    // Check the log level in order to avoid an unnecessary time-consuming string construction
    if (get_level() <= level::debug) {
        LogDebug(CommandMetaData::Instance().LogCdb(local_cdb, "SG driver"));
    }

    if (write && get_level() == level::trace && byte_count == remaining_count) {
        LogTrace(
            fmt::format("Transferring {0} byte(s) to SG driver:\n{1}", length, FormatBytes(buf, length, 128)));
    }

    int status = ioctl(fd, SG_IO, &io_hdr) == -1 ? -1 : io_hdr.status;

    format_header.clear();

    if (status == -1) {
        LogError(fmt::format("Transfer of {0} byte(s) failed: {1}", length, strerror(errno)));
        throw scsi_exception(sense_key::aborted_command, write ? asc::write_error : asc::read_error);
    }

    if (!status) {
        status = static_cast<int>(sense_data[2]) & 0x0f;

        if (static_cast<scsi_command>(GetController()->GetCdb()[0]) == scsi_command::inquiry
            && GetController()->GetEffectiveLun()) {
            // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
            buf[0] = 0x7f;
        }
    }

    if (status) {
        memcpy(deferred_sense_data.data(), sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = true;

        // This is just to set the return status to CHECK CONDITION
        throw scsi_exception(sense_key::no_sense);
    }

    if (!write && get_level() == level::trace && byte_count == remaining_count) {
        LogTrace(fmt::format("Transferred {0} byte(s) from SG driver:\n{1}", length,
            FormatBytes(buf, length, 128)));
    }

    UpdateInternalBlockSize(buf, length);

    UpdateStartBlock(length / block_size);

    remaining_count -= length;

    return length - io_hdr.resid;
}

int ScsiGeneric::GetAllocationLength() const
{
    const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(local_cdb[0]));

    // For commands without allocation length field the length is coded as a negative offset
    if (meta_data.allocation_length_offset < 0) {
        return -meta_data.allocation_length_offset;
    }

    switch (meta_data.allocation_length_size) {
    case 0:
        break;

    case 1:
        return local_cdb[meta_data.allocation_length_offset];

    case 2:
        return GetInt16(local_cdb, meta_data.allocation_length_offset);

    case 4:
        return GetInt32(local_cdb, meta_data.allocation_length_offset);

    default:
        assert(false);
        break;
    }

    return 0;
}

void ScsiGeneric::UpdateStartBlock(int length)
{
    switch (const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(local_cdb[0])); meta_data.block_size) {
    case 3:
        SetInt24(local_cdb, meta_data.block_offset, GetInt24(local_cdb, meta_data.block_offset) + length);
        break;

    case 4:
        SetInt32(local_cdb, meta_data.block_offset, GetInt32(local_cdb, meta_data.block_offset) + length);
        break;

    case 8:
        SetInt64(local_cdb, meta_data.block_offset, GetInt64(local_cdb, meta_data.block_offset) + length);
        break;

    default:
        break;
    }
}

void ScsiGeneric::SetBlockCount(int length)
{
    const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(local_cdb[0]));
    if (meta_data.block_size) {
        switch (meta_data.allocation_length_size) {
        case 1:
            local_cdb[meta_data.allocation_length_offset] = static_cast<uint8_t>(length);
            break;

        case 2:
            SetInt16(local_cdb, meta_data.allocation_length_offset, length);
            break;

        case 4:
            SetInt32(local_cdb, meta_data.allocation_length_offset, length);
            break;

        default:
            break;
        }
    }
}

void ScsiGeneric::UpdateInternalBlockSize(span<uint8_t> buf, int length)
{
    uint32_t size = block_size;
    if (const auto cmd = static_cast<scsi_command>(local_cdb[0]); cmd == scsi_command::read_capacity_10 && length >= 8) {
        size = GetInt32(buf, 4);
    }
    else if (cmd == scsi_command::read_capacity_16_read_long_16 && (static_cast<int>(local_cdb[1]) & 0x10) && length >= 12) {
        size = GetInt32(buf, 8);
    }

    if (block_size != size) {
        LogTrace(fmt::format("Updating internal block size to {} bytes", size));
        assert(size);
        if (size) {
            block_size = size;
        }
    }
}

void ScsiGeneric::SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    buf[offset] = static_cast<uint8_t>(value >> 16);
    buf[offset + 1] = static_cast<uint8_t>(value >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}

#endif
