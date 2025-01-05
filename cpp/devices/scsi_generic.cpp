//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
// Implementation of a generic SCSI device, using the Linux SG driver
//
//---------------------------------------------------------------------------

#include "scsi_generic.h"
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include "shared/sg_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;
using namespace sg_util;

ScsiGeneric::ScsiGeneric(int lun, const string &d) : PrimaryDevice(SCSG, lun), device(d)
{
    SupportsParams(true);
    SetReady(true);
}

string ScsiGeneric::SetUp()
{
    try {
        fd = OpenDevice(device);
    }
    catch (const IoException &e) {
        return e.what();
    }

    if (const string &error = GetDeviceData(); !error.empty()) {
        CleanUp();
        return "Can't get product data: " + error;
    }

    GetBlockSize();

    return "";
}

void ScsiGeneric::CleanUp()
{
    if (fd != -1) {
        close(fd);
    }
}

string ScsiGeneric::SetProductData(const ProductData &product_data, bool)
{
    return
        product_data.vendor.empty() && product_data.product.empty() && product_data.revision.empty() ?
            "" : "The product data of SCSG can't be changed";
}

void ScsiGeneric::Dispatch(ScsiCommand cmd)
{
    count = command_meta_data.GetByteCount(cmd);
    if (!count) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
    }

    local_cdb.resize(count);
    for (int i = 0; i < count; ++i) {
        local_cdb[i] = static_cast<uint8_t>(GetCdbByte(i));
    }

    // Convert READ/WRITE(6) to READ/WRITE(10) because some drives do not support READ/WRITE(6)
    if (cmd == ScsiCommand::READ_6 || cmd == ScsiCommand::WRITE_6) {
        local_cdb.push_back(0);
        // Sector count
        local_cdb.push_back(0);
        local_cdb.push_back(local_cdb[4]);
        // Control
        local_cdb.push_back(local_cdb[5]);
        // Sector number
        SetInt32(local_cdb, 2, GetInt24(local_cdb, 1));
        local_cdb[1] = 0;
        local_cdb[0] = cmd == ScsiCommand::WRITE_6 ? 0x2a : 0x28;
    }

    const auto &meta_data = command_meta_data.GetCdbMetaData(cmd);

    byte_count = meta_data.block_size ? GetAllocationLength(local_cdb) * block_size : GetAllocationLength(local_cdb);

    // FORMAT UNIT is special because the parameter list length can be part of the data sent with DATA OUT
    if (cmd == ScsiCommand::FORMAT_UNIT && (static_cast<int>(local_cdb[1]) & 0x10)) {
        // There must at least be the format list header, which has to be evaluated at the beginning of DATA OUT
        byte_count = 4;
    }

    remaining_count = byte_count;

    // There is no explicit LUN support, the SG driver maps each LUN to a device file
    if (GetController()->GetEffectiveLun() && cmd != ScsiCommand::INQUIRY) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::LOGICAL_UNIT_NOT_SUPPORTED);
    }

    auto &buf = GetController()->GetBuffer();

    if (cmd == ScsiCommand::REQUEST_SENSE && deferred_sense_data_valid) {
        memcpy(buf.data(), deferred_sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = false;

        const int length = min(18, byte_count);
        GetController()->SetTransferSize(length, length);

        DataInPhase(length);

        // REQUEST SENSE does not fail
        return;
    }

    deferred_sense_data_valid = false;

    // Split the transfer into chunks
    const int chunk_size = byte_count < MAX_TRANSFER_LENGTH ? byte_count : MAX_TRANSFER_LENGTH;

    GetController()->SetTransferSize(byte_count, chunk_size);

    // FORMAT UNIT needs special handling because of its implicit DATA OUT phase
    if (meta_data.has_data_out) {
        DataOutPhase(chunk_size || cmd != ScsiCommand::FORMAT_UNIT ? chunk_size : -1);
    }
    else {
        GetController()->SetCurrentLength(byte_count);
        DataInPhase(ReadData(buf));
    }
}

vector<uint8_t> ScsiGeneric::InquiryInternal() const
{
    assert(false);
    return {};
}

int ScsiGeneric::ReadData(data_in_t buf)
{
    return ReadWriteData(buf, GetController()->GetChunkSize());
}

int ScsiGeneric::WriteData(cdb_t, data_out_t buf, int, int length)
{
    // Evaluate the FORMAT UNIT format list header with the first chunk, send the command when all paramaeters are available
    if (static_cast<ScsiCommand>(local_cdb[0]) == ScsiCommand::FORMAT_UNIT
        && (static_cast<int>(local_cdb[1]) & 0x10)) {
        if (format_header.empty()) {
            format_header.push_back(buf[0]);
            format_header.push_back(buf[1]);
            format_header.push_back(buf[2]);
            format_header.push_back(buf[3]);
            byte_count = GetInt16(buf, 2) + 4;
            GetController()->SetTransferSize(byte_count, byte_count);
            return 0;
        }
        else {
            for (int i = 4; i < length; ++i) {
                format_header.push_back(buf[i - 4]);
            }
            buf = format_header;
            remaining_count = byte_count;
        }
    }

    return ReadWriteData(span((uint8_t*)buf.data(), buf.size()), length); // NOSONAR Cast required for SG driver API
}

int ScsiGeneric::ReadWriteData(span<uint8_t> buf, int chunk_size)
{
    int length = remaining_count < chunk_size ? remaining_count : chunk_size;
    length = length < MAX_TRANSFER_LENGTH ? length : MAX_TRANSFER_LENGTH;
    SetBlockCount(local_cdb, length / block_size);

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    const bool write = command_meta_data.GetCdbMetaData(static_cast<ScsiCommand>(local_cdb[0])).has_data_out;

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
        local_cdb[0] == static_cast<uint8_t>(ScsiCommand::FORMAT_UNIT) ?
            TIMEOUT_FORMAT_SECONDS : TIMEOUT_DEFAULT_SECONDS) * 1000;

    // Check the log level in order to avoid an unnecessary time-consuming string construction
    if (GetController() && GetLogger().level() <= level::debug) {
        LogDebug(command_meta_data.LogCdb(local_cdb, "SG driver"));
    }

    if (write && GetController() && GetLogger().level() == level::trace) {
        LogTrace(fmt::format("Transferring {0} byte(s) to SG driver{1}", length,
            length ? fmt::format(":\n{}", GetController()->FormatBytes(buf, length)) : ""));
    }

    const int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;

    format_header.clear();

    EvaluateStatus(status, span(buf.data(), length), sense_data, write);

    const int transferred_length = length - io_hdr.resid;

    if (!write && GetController() && GetLogger().level() == level::trace) {
        LogTrace(fmt::format("Transferred {0} byte(s) from SG driver{1}", transferred_length,
            transferred_length ? fmt::format(":\n{}", GetController()->FormatBytes(buf, transferred_length)) : ""));
    }

    UpdateInternalBlockSize(buf, length);

    UpdateStartBlock(local_cdb, length / block_size);

    // The remaining count for non-block oriented commands is 0 because there may be less than allocation length bytes
    if (command_meta_data.GetCdbMetaData(static_cast<ScsiCommand>(local_cdb[0])).block_size) {
        remaining_count -= transferred_length;
    }
    else {
        remaining_count = 0;
    }

    if (GetController()) {
        LogTrace(fmt::format("{0} byte(s) transferred, {1} byte(s) remaining", transferred_length, remaining_count));
    }

    return transferred_length;
}

void ScsiGeneric::EvaluateStatus(int status, span<uint8_t> buf, span<const uint8_t> sense_data, bool write)
{
    if (status == -1) {
        if (GetController()) {
            LogError(fmt::format("Transfer of {0} byte(s) failed: {1}", buf.size(), strerror(errno)));
        }

        throw ScsiException(SenseKey::ABORTED_COMMAND, write ? Asc::WRITE_ERROR : Asc::READ_ERROR);
    }
    // Do not consider CONDITION MET an error
    else if (status == static_cast<int>(StatusCode::CONDITION_MET)) {
        status = static_cast<int>(StatusCode::GOOD);
    }

    if (!status) {
        status = static_cast<int>(sense_data[2]) & 0x0f;

        if (static_cast<ScsiCommand>(local_cdb[0]) == ScsiCommand::INQUIRY && GetController()
            && GetController()->GetEffectiveLun()) {
            // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
            buf[0] = 0x7f;
        }
    }

    if (status) {
        memcpy(deferred_sense_data.data(), sense_data.data(), deferred_sense_data.size());
        deferred_sense_data_valid = true;

        // Set the return status to CHECK CONDITION
        throw ScsiException(SenseKey::NO_SENSE);
    }
}

void ScsiGeneric::UpdateInternalBlockSize(span<uint8_t> buf, int length)
{
    uint32_t size = block_size;
    if (const auto cmd = static_cast<ScsiCommand>(local_cdb[0]); cmd == ScsiCommand::READ_CAPACITY_10 && length >= 8) {
        size = GetInt32(buf, 4);
    }
    else if (cmd == ScsiCommand::READ_CAPACITY_READ_LONG_16 && (static_cast<int>(local_cdb[1]) & 0x10) && length >= 12) {
        size = GetInt32(buf, 8);
    }

    if (block_size != size) {
        LogTrace(fmt::format("Updating internal block size to {} bytes", size));
        if (size) {
            block_size = size;
        }
    }
}

string ScsiGeneric::GetDeviceData()
{
    vector<uint8_t> buf(36);

    byte_count = static_cast<int>(buf.size());
    remaining_count = byte_count;

    local_cdb.resize(6);
    fill_n(local_cdb.begin(), local_cdb.size(), 0);
    local_cdb[0] = static_cast<uint8_t>(ScsiCommand::INQUIRY);
    local_cdb[4] = static_cast<uint8_t>(byte_count);

    try {
        ReadWriteData(buf, byte_count);
    }
    catch (const ScsiException &e) {
        return e.what();
    }

    const auto& [vendor, product, revision] = GetInquiryProductData(buf);
    PrimaryDevice::SetProductData( { vendor, product, revision }, true);

    SetScsiLevel(static_cast<ScsiLevel>(buf[2]));
    SetResponseDataFormat(static_cast<ScsiLevel>(buf[3]));

    return "";
}

void ScsiGeneric::GetBlockSize()
{
    vector<uint8_t> buf(8);

    byte_count = static_cast<int>(buf.size());
    remaining_count = byte_count;

    local_cdb.resize(10);
    fill_n(local_cdb.begin(), local_cdb.size(), 0);
    local_cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CAPACITY_10);

    try {
        // Trigger a block size update
        ReadWriteData(buf, byte_count);
    }
    catch (const ScsiException&) { // NOSONAR The exception details do not matter, this might not be a block device
        // Fall through
    }
}
