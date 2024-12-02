//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Implementation of a generic SCSI device, using the Linux SG driver
//
//---------------------------------------------------------------------------

#ifdef __linux__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include "buses/bus_factory.h"
#include "shared/s2p_exceptions.h"
#include "scsi_generic.h"

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
    if (device.empty()) {
        LogError(fmt::format("Missing device file parameter"));
        return false;
    }

    if (!GetAsUnsignedInt(GetParam(TIMEOUT), timeout) || !timeout) {
        LogError(fmt::format("Invalid timeout value '{}'", GetParam(TIMEOUT)));
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
    count = BusFactory::Instance().GetCommandBytesCount(cmd);
    assert(count);

    cdb.clear();
    for (int i = 0; i < count; i++) {
        cdb.push_back(static_cast<uint8_t>(GetController()->GetCdb()[i]));
    }

    if (const auto &meta_data = BusFactory::Instance().GetCdbMetaData(cmd); meta_data.block_size)
    {
        byte_count = GetAllocationLength() * block_size;
    }
    else {
        byte_count = GetAllocationLength();
    }
    remaining_count = byte_count;

    // There is no explicit LUN support, the SG driver maps each LUN to a device file
    if (GetController()->GetEffectiveLun() && cmd != scsi_command::inquiry) {
        if (cmd != scsi_command::request_sense) {
            throw scsi_exception(sense_key::illegal_request, asc::logical_unit_not_supported);
        }

        auto &buf = GetController()->GetBuffer();

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
        memcpy(GetController()->GetBuffer().data(), deferred_sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = false;

        const int length = min(18, byte_count);
        GetController()->SetTransferSize(length, length);

        DataInPhase(length);

        // REQUEST SENSE does not fail
        return;
    }

    deferred_sense_data_valid = false;

    const int chunk_size = byte_count < MAX_TRANSFER_LENGTH ? byte_count : MAX_TRANSFER_LENGTH;

    // Split the transfer into chunks of MAX_TRANFER_LENGTH bytes
    GetController()->SetTransferSize(byte_count, chunk_size);

    if (WRITE_COMMANDS.contains(cmd)) {
        DataOutPhase(chunk_size);
    }
    else {
        GetController()->SetCurrentLength(byte_count);
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
}

param_map ScsiGeneric::GetDefaultParams() const
{
    return {
        {   DEVICE, ""},
        {   TIMEOUT, "3"}
    };
}

vector<uint8_t> ScsiGeneric::InquiryInternal() const
{
    assert(false);
    return {};
}

int ScsiGeneric::ReadData(data_in_t buf)
{
    return ReadWriteData(buf.data(), false, GetController()->GetChunkSize());
}

void ScsiGeneric::WriteData(data_out_t buf, scsi_command, int chunk_size)
{
    ReadWriteData((void*)buf.data(), true, chunk_size);
}

int ScsiGeneric::ReadWriteData(void *buf, bool write, int chunk_size) // NOSONAR SG driver API requires void *
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
    io_hdr.dxferp = io_hdr.dxfer_len ? buf : nullptr;

    array<uint8_t, 18> sense_data = { };
    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = sense_data.size();

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    LogTrace(fmt::format("Executing command ${0:02x} with SG driver, transfer length is {1} byte(s)", cdb[0], length));

    int status = ioctl(fd, SG_IO, &io_hdr) == -1 ? -1 : io_hdr.status;
    if (status == -1) {
        LogError(fmt::format("SCSI transfer of {0} byte(s) failed: {1}", length, strerror(errno)));
        throw scsi_exception(sense_key::aborted_command, write ? asc::write_error : asc::read_error);
    }

    if (!status) {
        status = static_cast<int>(sense_data[2]) & 0x0f;

        if (static_cast<scsi_command>(GetController()->GetCdb()[0]) == scsi_command::inquiry
            && GetController()->GetEffectiveLun()) {
            // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
            GetController()->GetBuffer().data()[0] = 0x7f;
        }
    }

    if (status) {
        memcpy(deferred_sense_data.data(), sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = true;

        // This is just to set the return status to CHECK CONDITION
        throw scsi_exception(sense_key::no_sense);
    }

    UpdateStartBlock(length / block_size);

    remaining_count -= length;

    return length - io_hdr.resid;
}

int ScsiGeneric::GetAllocationLength() const
{
    const auto &meta_data = BusFactory::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0]));

    // For commands without allocation length field the length is coded as a negative offset
    if (meta_data.allocation_length_offset < 0) {
        return -meta_data.allocation_length_offset;
    }

    switch (meta_data.allocation_length_size) {
    case 0:
        break;

    case 1:
        return cdb[meta_data.allocation_length_offset];

    case 2:
        return GetInt16(cdb, meta_data.allocation_length_offset);

    case 4:
        return GetInt32(cdb, meta_data.allocation_length_offset);

    default:
        assert(false);
        break;
    }

    return 0;
}

void ScsiGeneric::UpdateStartBlock(int length)
{
    switch (const auto &meta_data = BusFactory::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])); meta_data.block_size) {
    case 3:
        SetInt24(cdb, meta_data.block_offset, GetInt24(cdb, meta_data.block_offset) + length);
        break;

    case 4:
        SetInt32(cdb, meta_data.block_offset, GetInt32(cdb, meta_data.block_offset) + length);
        break;

    case 8:
        SetInt64(cdb, meta_data.block_offset, GetInt64(cdb, meta_data.block_offset) + length);
        break;

    default:
        break;
    }
}

void ScsiGeneric::SetBlockCount(int length)
{
    switch (const auto &meta_data = BusFactory::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])); meta_data.allocation_length_size) {
    case 1:
        cdb[meta_data.allocation_length_offset] = static_cast<uint8_t>(length);
        break;

    case 2:
        SetInt16(cdb, meta_data.allocation_length_offset, length);
        break;

    case 4:
        SetInt32(cdb, meta_data.allocation_length_offset, length);
        break;

    default:
        break;
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
