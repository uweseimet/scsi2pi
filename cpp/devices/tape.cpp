//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// The SCTP device is a SCSI-2 sequential access device with some SSC-5 command extensions.
//
// Files with a .tar extension are treated in a special way, so that unmodified .tar files can be used an image
// files. For .tar files, filemark, end-of-data, spacing, formatting and erasing are not supported.
// Setmarks are not supported. They have been removed from the standard since SSC-2.
// With SIMH-compatible .tap files (see https://simh.trailing-edge.com/docs/simh_magtape.pdf) all SCSI2Pi tape
// device features are supported.
// Note that tar (actually the Linux tape device driver) tries to create a filemark as an end-of-data marker when
// writing to a tape device, but not when writing to a local .tar file.
//
//---------------------------------------------------------------------------

#include "tape.h"
#include "base/property_handler.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Tape::Tape(int lun) : StorageDevice(SCTP, lun, true, false, { 512, 1024, 2048, 4096, 8192 })
{
    StorageDevice::SetProductData( { "", "SCSI TAPE", "" });
    SupportsParams(true);
    SetProtectable(true);
    SetRemovable(true);
}

bool Tape::SetUp()
{
    AddCommand(scsi_command::read_6, [this]
        {
            Read(false);
        });
    AddCommand(scsi_command::read_16, [this]
        {
            Read(true);
        });
    AddCommand(scsi_command::write_6, [this]
        {
            Write(false);
        });
    AddCommand(scsi_command::write_16, [this]
        {
            Write(true);
        });
    AddCommand(scsi_command::erase_6, [this]
        {
            Erase6();
        });
    AddCommand(scsi_command::read_block_limits, [this]
        {
            ReadBlockLimits();
        });
    AddCommand(scsi_command::rewind, [this]
        {
            Rewind();
        });
    AddCommand(scsi_command::space_6, [this]
        {
            Space6();
        });
    AddCommand(scsi_command::write_filemarks_6, [this]
        {
            WriteFilemarks(false);
        });
    AddCommand(scsi_command::write_filemarks_16, [this]
        {
            WriteFilemarks(true);
        });
    AddCommand(scsi_command::locate_10, [this]
        {
            Locate(false);
            StatusPhase();
        });
    AddCommand(scsi_command::locate_16, [this]
        {
            Locate(true);
            StatusPhase();
        });
    AddCommand(scsi_command::read_position, [this]
        {
            ReadPosition();
        });
    AddCommand(scsi_command::format_medium, [this]
        {
            FormatMedium();
        });

    return StorageDevice::SetUp();
}

void Tape::CleanUp()
{
    StorageDevice::CleanUp();

    file.close();
}

void Tape::ValidateFile()
{
    StorageDevice::ValidateFile();

    file.open(GetFilename(), ios::in | ios::out | ios::binary);
    if (file.bad()) {
        throw io_exception("Can't open image file '" + GetFilename() + "'");
    }

    tar_file = GetExtensionLowerCase(GetFilename()) == "tar";
}

void Tape::Read(bool read_16)
{
    // FIXED and SILI must not both be set, only partition 0 is supported
    if ((GetCdbByte(1) & 0b11) == 0b11 || (read_16 && GetCdbByte(3))) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    expl = read_16;
    if (expl && !Locate(true)) {
        SetInformation(GetCdbInt24(12));
        throw scsi_exception(sense_key::no_sense, asc::locate_operation_failure);
    }

    byte_count = GetByteCount();
    if (byte_count) {
        blocks_read = 0;

        remaining_count = byte_count;

        initial = true;

        GetController()->SetTransferSize(byte_count, GetBlockSize());

        GetController()->SetCurrentLength(GetBlockSize());
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
    else {
        StatusPhase();
    }
}

void Tape::Write(bool write_16)
{
    // FCS and LCS are not supporte, only partition 0 is supported
    if (write_16 && (GetCdbByte(1) & 0b1100 || GetCdbByte(3))) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckWritePreconditions();

    expl = write_16;
    if (expl && !Locate(true)) {
        SetInformation(GetCdbInt24(12));
        throw scsi_exception(sense_key::no_sense, asc::locate_operation_failure);
    }

    byte_count = GetByteCount();
    if (byte_count) {
        remaining_count = byte_count;

        record_length = fixed ? GetBlockSize() : byte_count;

        initial = true;

        GetController()->SetTransferSize(byte_count, byte_count < GetBlockSize() ? byte_count : GetBlockSize());

        DataOutPhase(byte_count < GetBlockSize() ? byte_count : GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

int Tape::ReadData(data_in_t buf)
{
    CheckReady();

    const int length = GetController()->GetChunkSize();

    if (IsAtRecordBoundary()) {
        file.seekg(tape_position);

        current_meta_data = FindNextObject(object_type::block, 0, true);

        record_length = current_meta_data.value;

        if (current_meta_data.cls == simh_class::bad_data_record && !record_length) {
            ++read_error_count;
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        tape_position -= record_length + META_DATA_SIZE;

        CheckBlockLength();
    }

    LogTrace(
        fmt::format("Reading {0} data byte(s) from position {1}, record length is {2}", length, tape_position, record_length));

    file.seekg(tape_position);
    file.read((char*)buf.data(), length);
    CheckForReadError();

    remaining_count -= length;
    tape_position += length;

    if (IsAtRecordBoundary()) {
        tape_position += record_length > GetBlockSize() ? 0 : Pad(record_length) - length;

        // Trailing length
        array<uint8_t, META_DATA_SIZE> data;
        file.seekg(tape_position);
        file.read((char*)data.data(), data.size());
        CheckForReadError();

        if (const int trailing_length = FromLittleEndian(data).value; static_cast<int>(record_length)
            != trailing_length) {
            RaiseReadError(FromLittleEndian(data));
        }

        // Skip trailing length
        tape_position += META_DATA_SIZE;

        if (!remaining_count) {
            ++blocks_read;
        }
    }

    return length;
}

int Tape::WriteData(cdb_t, data_out_t buf, int, int chunk_size)
{
    CheckReady();

    if (IsAtRecordBoundary()) {
        WriteMetaData(object_type::block, record_length);
    }

    const uint32_t length =
        byte_count < static_cast<uint32_t>(chunk_size) ? byte_count : static_cast<uint32_t>(chunk_size);

    LogTrace(fmt::format("Writing {0} data byte(s) to position {1}, record length is {2}", length, tape_position,
        Pad(record_length)));

    CheckForOverflow(length);

    file.seekp(tape_position);
    file.write((const char*)buf.data(), length);
    CheckForWriteError();

    remaining_count -= length;
    tape_position += length;

    if (IsAtRecordBoundary()) {
        if (tape_position % 2) {
            file << '\0';
            ++tape_position;
        }

        tape_position += WriteSimhMetaData(simh_class::tape_mark_good_data_record, record_length);

        ++object_location;
    }

    if (!remaining_count) {
        // Ensure that there is always an end-of-data object
        WriteMetaData(object_type::end_of_data);
    }

    return chunk_size;
}

void Tape::Open()
{
    assert(!IsReady());

    // Block size and number of blocks
    if (!SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 512)) {
        throw io_exception("Invalid block size: " + to_string(GetConfiguredBlockSize()));
    }

    if (int append; !GetAsUnsignedInt(GetParam(APPEND), append)) {
        throw parser_exception(fmt::format("Invalid maximum file size: '{}'", GetParam(APPEND)));
    }
    else {
        max_file_size = append;
    }

    if (max_file_size && max_file_size < GetBlockSize()) {
        throw io_exception("Maximum file size " + to_string(max_file_size) + " is smaller than block size "
        + to_string(GetBlockSize()));
    }

    block_size_for_descriptor = GetBlockSize();

    file_size = GetFileSize(true);

    // In append mode, ensure that the image file size is at least the block size
    if (max_file_size && file_size < GetBlockSize()) {
        file.open(GetFilename(), ios::out | ios::binary);
        file.seekp(GetBlockSize() - 1);
        file.put(0);
        file.flush();
        if (file.bad()) {
            file.close();
            throw io_exception("Can't write to '" + GetFilename() + "'");
        }
        file.close();
    }

    if (!max_file_size && file_size) {
        max_file_size = file_size;
    }

    SetBlockCount(static_cast<uint32_t>(file_size / GetBlockSize()));

    ValidateFile();

    ResetPositions();
}

bool Tape::Eject(bool force)
{
    const bool status = StorageDevice::Eject(force);
    if (status) {
        file.close();

        read_error_count = 0;
        write_error_count = 0;
    }

    return status;
}

param_map Tape::GetDefaultParams() const
{
    return {
        {   APPEND, "0"}
    };
}

vector<uint8_t> Tape::InquiryInternal() const
{
    return HandleInquiry(device_type::sequential_access, true);
}

bool Tape::ValidateBlockSize(uint32_t size) const
{
    // Tape drives support multiples of 4
    return size && !(size % 4);
}

uint32_t Tape::VerifyBlockSizeChange(uint32_t requested_size, bool temporary)
{
    // Special handling of block size 0 for sequential-access devices, according to the SCSI-2 specification
    if (!requested_size && temporary) {
        block_size_for_descriptor = 0;
        return 0;
    }

    return
        requested_size || !temporary ? StorageDevice::VerifyBlockSizeChange(requested_size, temporary) : GetBlockSize();
}

void Tape::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    StorageDevice::SetUpModePages(pages, page, changeable);

    // Page 15 (data compression)
    if (page == 0x0f || page == 0x3f) {
        AddDataCompressionPage(pages);
    }

    // Page 16 (device configuration)
    if (page == 0x10 || page == 0x3f) {
        AddDeviceConfigurationPage(pages, changeable);
    }

    // Page 17 (medium partition page 1)
    if (page == 0x11 || page == 0x3f) {
        AddMediumPartitionPage(pages, changeable);
    }
}

void Tape::AddDataCompressionPage(map<int, vector<byte>> &pages) const
{
    vector<byte> buf(16);

    pages[15] = buf;
}

void Tape::AddDeviceConfigurationPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(16);

    if (!changeable) {
        // BIS/LOIS (logical block identifiers supported)
        buf[8] = byte { 0b01000000 };

        // EEG (enable EOD generation)
        buf[10] = byte { 0b00010000 };
    }

    pages[16] = buf;
}

void Tape::AddMediumPartitionPage(map<int, vector<byte> > &pages, bool changeable) const
{
    vector<byte> buf(8);

    if (!changeable) {
        // Fixed data partitions, PSUM (descriptor unit in MB)
        buf[4] = byte { 0b10010000 };
    }

    pages[17] = buf;
}

void Tape::Erase6()
{
    if (tar_file) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    CheckWritePreconditions();

    // Check Long bit. Like with an HP35470A a long erase erases everything, otherwise only EOD is written.
    if (GetCdbByte(1) & 0x01) {
        Erase();

        // After a Long erase according to the standard the position is undefined, for SCSI2Pi it is 0
        ResetPositions();
    }

    WriteMetaData(object_type::end_of_data);

    StatusPhase();
}

void Tape::ReadBlockLimits() const
{
    // Granularity and maximum block size
    SetInt32(GetController()->GetBuffer(), 0, 0x02fffffc);

    // Minimum block size
    SetInt16(GetController()->GetBuffer(), 4, 4);

    DataInPhase(6);
}

void Tape::Rewind()
{
    ResetPositions();

    StatusPhase();
}

void Tape::Space6()
{
    if (tar_file) {
        LogError("In tar-compatibility mode spacing is not supported");
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    switch (const auto code = static_cast<object_type>(GetCdbByte(1) & 0x07); code) {
    case object_type::block:
        if (const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2); count) {
            FindNextObject(code, count, false);
        }
        break;

    case object_type::filemark:
        if (const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2); count) {
            FindNextObject(code, count, false);
        }
        break;

    case object_type::end_of_data:
        FindNextObject(object_type::end_of_data, 0, false);
        tape_position -= META_DATA_SIZE;
        break;

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    StatusPhase();
}

void Tape::WriteFilemarks(bool write_filemarks_16)
{
    if (tar_file) {
        LogTrace("Writing filemarks in tar-compatibility mode is not supported, WRITE FILEMARKS command is ignored");
        StatusPhase();
        return;
    }

    // Since SSC-3 setmarks are not supported anymore, FCS/LCS are not supported, only partition 0 is supported
    if (GetCdbByte(1) & 0x02 || (write_filemarks_16 && (GetCdbByte(1) & 0b1100 || GetCdbByte(3)))) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckWritePreconditions();

    int count;
    if (write_filemarks_16) {
        if (const auto identifier = static_cast<uint32_t>(GetCdbInt64(4)); identifier) {
            ResetPositions();
            if (!FindObject(identifier)) {
                SetInformation(GetCdbInt24(12));
                throw scsi_exception(sense_key::no_sense, asc::locate_operation_failure);
            }
        }

        count = GetCdbInt24(12);
    }
    else {
        count = GetCdbInt24(2);
    }

    for (int i = 0; i < count; i++) {
        WriteMetaData(object_type::filemark);
    }

    StatusPhase();
}

bool Tape::Locate(bool locate_16)
{
    // CP is not supported
    if (GetCdbByte(1) & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    auto identifier = static_cast<uint32_t>(locate_16 ? GetCdbInt64(4) : GetCdbInt32(3));
    const bool bt = GetCdbByte(1) & 0x04;

    if (tar_file) {
        if (bt) {
            // The device-specific identifier must be a multiple of the block size
            if (identifier % GetBlockSize()) {
                throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
            }

            tape_position = identifier;
            object_location = identifier / GetBlockSize();
        }
        else {
            tape_position = identifier * GetBlockSize();
            object_location = identifier;
        }
    }
    else {
        if (bt && identifier) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
        } else {
            ResetPositions();
            if (identifier) {
                return FindObject(identifier);
            }
        }
    }

    return true;
}

void Tape::ReadPosition() const
{
    auto &buf = GetController()->GetBuffer();
    fill_n(buf.begin(), 20, 0);

    // BOP
    if (!tape_position) {
        buf[0] = 0b10000000;
    }

    // EOP
    if (tape_position >= file_size) {
        buf[0] += 0b01000000;
    }

    // Partition number is always 0

    // BT (SCSI-2)/service action 01 (since SSC-2)
    const bool bt = GetCdbByte(1) & 0x01;
    SetInt32(buf, 4, static_cast<uint32_t>(bt ? tape_position : object_location));
    SetInt32(buf, 8, static_cast<uint32_t>(bt ? tape_position : object_location));

    DataInPhase(20);
}

void Tape::FormatMedium()
{
    if (tar_file) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    CheckWritePreconditions();

    if (tape_position) {
        throw scsi_exception(sense_key::illegal_request, asc::sequential_positioning_error);
    }

    Erase();

    ResetPositions();

    WriteMetaData(object_type::end_of_data);

    StatusPhase();
}

void Tape::WriteMetaData(Tape::object_type type, uint32_t size)
{
    if (tar_file) {
        return;
    }

    assert(!(size & 0xf0000000));

    file.seekp(tape_position);

    if (type == object_type::block || type == object_type::filemark) {
        tape_position += WriteSimhMetaData(simh_class::tape_mark_good_data_record, size);
    }

    // Ensure that there is always an end-of-data object behind the last position
    if (file_size >= tape_position + META_DATA_SIZE) {
        WriteSimhMetaData(simh_class::private_marker,
            (static_cast<uint32_t>(object_type::end_of_data) << 24) | PRIVATE_MARKER_MAGIC);
    }

    CheckForWriteError();
}

SimhMetaData Tape::FindNextObject(object_type type, int32_t requested_count, bool read)
{
    const bool reverse = requested_count < 0;

    LogTrace(fmt::format("{0}-spacing at position {1} for object type {2}, count {3}", reverse ? "Reverse" : "Forward",
        tape_position, static_cast<int>(type), requested_count));

    if (reverse) {
        requested_count = -requested_count;
    }

    int32_t actual_count = 0;

    while (true) {
        SimhMetaData meta_data;
        const auto [scsi_type, length] = ReadSimhMetaData(meta_data, requested_count, reverse);

        // Bad data (not recovered) during READ(6)
        if (read && scsi_type == object_type::block && !length) {
            return meta_data;
        }

        LogTrace(
            fmt::format("Found object type {0}, length {1}, spaced over {2} object(s)", static_cast<int>(scsi_type),
                length, actual_count));

        if (!reverse) {
            tape_position += IsRecord(meta_data) ? Pad(meta_data.value) + META_DATA_SIZE : 0;
        }

        if (scsi_type == object_type::end_of_data) {
            if (type == object_type::end_of_data) {
                return meta_data;
            }
            else {
                // End-of-data while spacing over something else
                RaiseEndOfData(type, requested_count);
            }
        }

        // For end-of-data the count is ignored
        if (type == object_type::end_of_data) {
            continue;
        }

        if (scsi_type == object_type::filemark && type == object_type::block) {
            // Terminate while spacing over blocks and a filemark is found
            RaiseFilemark(requested_count, read);
        }

        if (scsi_type == type) {
            --requested_count;
            ++actual_count;
        }

        if (requested_count <= 0) {
            return meta_data;
        }
    }
}

void Tape::RaiseBeginningOfPartition(int32_t info)
{
    LogTrace("Encountered beginning-of-partition while reverse-spacing");

    ResetPositions();
    SetInformation(info);
    SetEom(ascq::beginning_of_partition_medium_detected);

    throw scsi_exception(sense_key::no_sense);
}

void Tape::RaiseEndOfPartition(int32_t info)
{
    LogTrace(fmt::format("Encountered end-of-partition at position {} while spacing", tape_position));

    SetInformation(info);
    SetEom(ascq::end_of_partition_medium_detected);

    throw scsi_exception(sense_key::medium_error, asc::no_additional_sense_information);
}

void Tape::RaiseEndOfData(object_type type, int32_t info)
{
    tape_position -= META_DATA_SIZE;

    LogTrace(fmt::format("Encountered end-of-data at position {0} while spacing over object type {1}", tape_position,
        static_cast<int>(type)));

    SetInformation(info);

    throw scsi_exception(sense_key::blank_check, asc::no_additional_sense_information);
}

void Tape::RaiseFilemark(int32_t info, bool read)
{
    LogTrace(fmt::format("Encountered filemark at position {} while spacing over blocks", tape_position));

    if (read && !fixed) {
        SetInformation(GetByteCount());
    }
    else {
        SetInformation(info);
    }
    SetFilemark();

    throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
}

// TODO Raise a read error with information field set also for other read errors
void Tape::RaiseReadError(const SimhMetaData &meta_data)
{
    LogError(fmt::format("Trailing record length {0} at position {1} does not match leading length {2}",
        meta_data.value, tape_position, record_length));

    tape_position += META_DATA_SIZE;
    ++blocks_read;
    ++read_error_count;

    SetInformation(fixed ? blocks_read : byte_count);

    throw scsi_exception(sense_key::medium_error, asc::read_error);
}

void Tape::ResetPositions()
{
    tape_position = 0;
    object_location = 0;
}

bool Tape::ReadNextMetaData(SimhMetaData &meta_data, bool reverse)
{
    if (reverse) {
        // Position before trailing length or marker
        tape_position -= META_DATA_SIZE;

        if (tape_position < 0) {
            return false;
        }

        file.seekg(tape_position);
        if (!ReadMetaData(file, meta_data)) {
            ++read_error_count;
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }
        tape_position -= IsRecord(meta_data) ? Pad(meta_data.value) + META_DATA_SIZE : 0;
    }
    else {
        file.seekg(tape_position);
        if (!ReadMetaData(file, meta_data)) {
            ++read_error_count;
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }
        tape_position += META_DATA_SIZE;
    }

    LogTrace(fmt::format("Read SIMH meta data with class {0:1X}, value ${1:07x} at position {2}",
        static_cast<int>(meta_data.cls), meta_data.value, reverse ? tape_position : tape_position - META_DATA_SIZE));

    return true;
}

bool Tape::FindObject(uint32_t identifier)
{
    while (true) {
        SimhMetaData meta_data;
        ReadSimhMetaData(meta_data, 0, false);

        if (meta_data.cls == simh_class::private_marker && (meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC) {
            return false;
        }

        if (IsRecord(meta_data) || (meta_data.cls == simh_class::bad_data_record && !meta_data.value)
            || (meta_data.cls == simh_class::tape_mark_good_data_record && !meta_data.value)) {
            tape_position += IsRecord(meta_data) ? Pad(meta_data.value) + META_DATA_SIZE : 0;

            --identifier;
            if (!identifier) {
                return true;
            }
        }
    }
}

uint32_t Tape::GetByteCount()
{
    fixed = GetCdbByte(1) & 0x01;

    // Drive is not in fixed-length mode
    if (fixed && !block_size_for_descriptor) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int length = GetInt24(GetController()->GetCdb(), expl ? 12 : 2);
    const int32_t count = fixed ? length * GetBlockSize() : length;

    LogTrace(fmt::format("Current position: {0}, requested byte count: {1}", tape_position, count));

    return count;
}

void Tape::Erase()
{
    file.seekp(tape_position);

    // Erase in chunks, using SIMH gaps as a pattern (little endian)
    vector<uint8_t> buf;
    buf.reserve(1024 * sizeof(simh_marker::erase_gap));
    const auto gap = static_cast<uint32_t>(simh_marker::erase_gap);
    for (int i = 0; i < 1024; i++) {
        buf.push_back(gap & 0xff);
        buf.push_back((gap >> 8) & 0xff);
        buf.push_back((gap >> 16) & 0xff);
        buf.push_back((gap >> 24) & 0xff);
    }

    uint64_t remaining = file_size - tape_position;
    while (remaining >= 4) {
        uint64_t chunk = remaining;
        if (chunk > buf.size()) {
            chunk = buf.size();
        }

        file.write((const char*)buf.data(), chunk);
        CheckForWriteError();

        remaining -= chunk;
        tape_position += chunk;
        object_location += chunk / GetBlockSize();
    }
}

vector<PbStatistics> Tape::GetStatistics() const
{
    vector<PbStatistics> statistics = StorageDevice::GetStatistics();

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());
    s.set_category(PbStatisticsCategory::CATEGORY_ERROR);

    s.set_key(READ_ERROR_COUNT);
    s.set_value(read_error_count);
    statistics.push_back(s);

    if (!IsReadOnly()) {
        s.set_key(WRITE_ERROR_COUNT);
        s.set_value(write_error_count);
        statistics.push_back(s);
    }

    return statistics;
}

pair<Tape::object_type, int> Tape::ReadSimhMetaData(SimhMetaData &meta_data, int32_t count, bool reverse)
{
    while (true) {
        if (!ReadNextMetaData(meta_data, reverse)) {
            RaiseBeginningOfPartition(count);
        }

        // Update object location for data records and tape marks
        if (IsRecord(meta_data) || (meta_data.cls == simh_class::bad_data_record && !meta_data.value)
            || (meta_data.cls == simh_class::tape_mark_good_data_record && !meta_data.value)) {
            object_location += reverse ? -1 : 1;
        }

        switch (meta_data.cls) {
        case simh_class::tape_mark_good_data_record:
            return {meta_data.value ? object_type::block : object_type::filemark, meta_data.value};

        case simh_class::bad_data_record:
            return {object_type::block, meta_data.value};

        case simh_class::reserved_marker:
            if (meta_data.value == static_cast<int>(simh_marker::end_of_medium)) {
                RaiseEndOfPartition(count);
            }
            LogTrace(
                meta_data.value == static_cast<int>(simh_marker::erase_gap) ?
                    "Skipping SIMH erase gap" : "Skipping unknown SIMH reserved marker");
            break;

        case simh_class::private_marker:
            if ((meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC) {
                LogTrace(fmt::format("Found SCSI2Pi private marker for object type {0} at position {1}",
                    (meta_data.value >> 24) & 0x0f, tape_position - META_DATA_SIZE));
                return {static_cast<object_type>((meta_data.value >> 24) & 0x0f), 0};
            }
            LogTrace(fmt::format("Skipping unknown SIMH private marker, value ${0:07x} at position {1}",
                static_cast<int>(meta_data.value), tape_position - META_DATA_SIZE));
            break;

        default:
            LogTrace(fmt::format("Skipping unknown SIMH class {0:1X} at position {1}",
                static_cast<int>(meta_data.cls), tape_position - META_DATA_SIZE));
            if (!reverse && IsRecord(meta_data)) {
                tape_position += Pad(meta_data.value) + META_DATA_SIZE;
            }
            break;
        }
    }
}

int Tape::WriteSimhMetaData(simh_class cls, uint32_t value)
{
    LogTrace(
        fmt::format("Writing SIMH meta data with class {0:1X}, value ${1:07x} to position {2}", static_cast<int>(cls),
            value, tape_position));

    CheckForOverflow(tape_position + META_DATA_SIZE);

    file.write((const char*)ToLittleEndian( { cls, value }).data(), META_DATA_SIZE);
    CheckForWriteError();

    return META_DATA_SIZE;
}

void Tape::CheckBlockLength()
{
    if (record_length != byte_count) {
        // In fixed mode an incorrect length results in an error if it is not a multiple of the block size.
        // SSC-5: "If the FIXED bit is one, the INFORMATION field shall be set to the requested transfer length
        // minus the actual number of logical blocks read, not including the incorrect-length logical block."
        if (fixed && byte_count % record_length) {
            tape_position += record_length + META_DATA_SIZE;

            SetIli();
            SetInformation((byte_count - remaining_count) / GetBlockSize() - blocks_read);

            throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
        }

        // Report CHECK CONDITION if SILI is not set and the actual length does not match the requested length.
        // SSC-5: "If the FIXED bit is zero, the INFORMATION field shall be set to the requested transfer length
        // minus the actual logical block length."
        // If SILI is set report CHECK CONDITION for the overlength condition only.
        if (!fixed && (!(GetCdbByte(1) & 0x02) || byte_count > record_length)) {
            tape_position += record_length + META_DATA_SIZE;

            SetIli();
            SetInformation(byte_count - record_length);

            throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
        }
    }
}

bool Tape::IsAtRecordBoundary()
{
    if (tar_file) {
        return false;
    }

    const bool boundary = fixed || initial || !remaining_count || byte_count - remaining_count == record_length;

    initial = false;

    return boundary;
}

void Tape::CheckForOverflow(int64_t length)
{
    if (tape_position + length > max_file_size) {
        ++write_error_count;
        throw scsi_exception(sense_key::volume_overflow);
    }
}

void Tape::CheckForReadError()
{
    if (file.fail()) {
        file.clear();
        ++read_error_count;
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }
}

void Tape::CheckForWriteError()
{
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    file.flush();
}

int32_t Tape::GetSignedInt24(cdb_t buf, int offset)
{
    const int value = GetInt24(buf, offset);
    return value >= 0x800000 ? value - 0x1000000 : value;
}
