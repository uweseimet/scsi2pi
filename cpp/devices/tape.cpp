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

Tape::Tape(int lun) : StorageDevice(SCTP, scsi_level::scsi_2, lun, true, false, { 512, 1024, 2048, 4096, 8192 })
{
    SetProduct("SCSI TAPE");
    SupportsParams(true);
    SetProtectable(true);
    SetRemovable(true);
}

bool Tape::SetUp()
{
    AddCommand(scsi_command::read_6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::write_6, [this]
        {
            Write6();
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
            WriteFilemarks6();
        });
    AddCommand(scsi_command::locate_10, [this]
        {
            Locate(false);
        });
    AddCommand(scsi_command::locate_16, [this]
        {
            Locate(true);
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

void Tape::Read6()
{
    // FIXED and SILI must not both be set
    if ((GetCdbByte(1) & 0x03) == 0x03) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    byte_count = GetByteCount();
    if (byte_count) {
        blocks_read = 0;

        remaining_count = byte_count;

        // The record length is set from the SIMH meta data later
        record_length = 0;

        GetController()->SetTransferSize(byte_count, GetBlockSize());

        GetController()->SetCurrentLength(GetBlockSize());
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
    else {
        StatusPhase();
    }
}

void Tape::Write6()
{
    CheckWritePreconditions();

    byte_count = GetByteCount();
    if (byte_count) {
        remaining_count = byte_count;

        // TODO Verify
        record_length = 0;

        GetController()->SetTransferSize(byte_count, byte_count < GetBlockSize() ? byte_count : GetBlockSize());

        DataOutPhase(byte_count < GetBlockSize() ? byte_count : GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

int Tape::ReadData(span<uint8_t> buf)
{
    CheckReady();

    const int size = GetController()->GetChunkSize();

    if (IsAtRecordBoundary()) {
        file.seekg(position, ios::beg);

        record_length = FindNextObject(object_type::block, 0);

        position -= record_length + META_DATA_SIZE;

        CheckBlockLength(size);
    }

    LogTrace(
        fmt::format("Reading {0} data byte(s) from position {1}, record length is {2}", size, position, record_length));

    file.seekg(position, ios::beg);
    file.read((char*)buf.data(), size);
    CheckForReadError();

    remaining_count -= size;
    position += size;

    if (IsAtRecordBoundary()) {
        position += record_length > GetBlockSize() ? 0 : record_length - size + GetPadding(record_length);

        // Trailing length
        array<uint8_t, META_DATA_SIZE> data = { };
        file.seekg(position, ios::beg);
        file.read((char*)data.data(), data.size());
        CheckForReadError();

        if (const int trailing_length = FromLittleEndian(data).value; static_cast<int>(record_length)
            != trailing_length) {
            LogError(fmt::format("Trailing record length {0} at position {1} does not match leading length {2}",
                trailing_length, position, record_length));
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        // Skip trailing length
        position += META_DATA_SIZE;
    }

    if (!remaining_count) {
        ++block_location;
        ++blocks_read;
    }

    return size;
}

int Tape::WriteData(span<const uint8_t> buf, scsi_command)
{
    CheckReady();

    const uint32_t size =
        byte_count < static_cast<uint32_t>(GetController()->GetChunkSize()) ?
            byte_count : static_cast<uint32_t>(GetController()->GetChunkSize());

    if (IsAtRecordBoundary()) {
        WriteMetaData(object_type::block, size);
    }

    LogTrace(fmt::format("Writing {0} data byte(s) to position {1}, record length is {2}", size, position,
        record_length));

    file.seekp(position);
    file.write((const char*)buf.data(), size);
    CheckForWriteError();

    remaining_count -= size;
    position += size;
    if (!remaining_count) {
        ++block_location;
    }

    if (IsAtRecordBoundary()) {
        if (!remaining_count && GetPadding(record_length)) {
            file << '\0';
            ++position;
        }

        position += WriteSimhMetaData(simh_class::tape_mark_good_data_record, size);
    }

    if (!remaining_count) {
        // Ensure that there is always an end-of-data object
        WriteMetaData(object_type::end_of_data);
    }

    return size;
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

    ResetPosition();
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

int Tape::VerifyBlockSizeChange(int requested_size, bool temporary) const
{
    // Special handling of block size 0 for sequential-access devices, according to the SCSI-2 specification
    return
        requested_size || !temporary ? StorageDevice::VerifyBlockSizeChange(requested_size, temporary) : GetBlockSize();
}

void Tape::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    StorageDevice::SetUpModePages(pages, page, changeable);

    // Page 0 (mode block descriptor), used by tools like tar.
    // Due to its format page 0 cannot be returned with a page list. This has been verified with an HP 35470A.
    if (page == 0x00) {
        AddModeBlockDescriptor(pages);
    }

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

void Tape::AddModeBlockDescriptor(map<int, vector<byte>> &pages) const
{
    vector<byte> buf(12);

    // Page size, the size field does not count itself
    buf[0] = (byte)11;

    // WP
    if (IsProtected()) {
        buf[2] = (byte)0x80;
    }

    // Block descriptor length
    buf[3] = (byte)8;

    // Size of fixed blocks
    SetInt32(buf, 8, GetBlockSize());

    pages[0] = buf;
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
        buf[8] = (byte)0b01000000;

        // EEG (enable EOD generation)
        buf[10] = (byte)0b00010000;
    }

    pages[16] = buf;
}

void Tape::AddMediumPartitionPage(map<int, vector<byte> > &pages, bool changeable) const
{
    vector<byte> buf(8);

    if (!changeable) {
        // Fixed data partitions, PSUM (descriptor unit in MB)
        buf[4] = (byte)0b10010000;
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
        ResetPosition();
    }

    WriteMetaData(object_type::end_of_data);

    StatusPhase();
}

void Tape::ReadBlockLimits()
{
    SetInt32(GetController()->GetBuffer(), 0, *ranges::max_element(GetSupportedBlockSizes()));
    SetInt16(GetController()->GetBuffer(), 4, 4);

    DataInPhase(6);
}

void Tape::Rewind()
{
    ResetPosition();

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
    case object_type::filemark:
        if (const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2); count) {
            FindNextObject(code, count);
        }
        break;

    case object_type::end_of_data:
        FindNextObject(object_type::end_of_data, 0);
        break;

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    StatusPhase();
}

void Tape::WriteFilemarks6()
{
    if (tar_file) {
        LogTrace("Writing filemarks in tar-compatibility mode is not supported, WRITE FILEMARKS(6) command is ignored");
        StatusPhase();
        return;
    }

    // Since SSC-2 setmarks are not supported anymore
    if (GetCdbByte(1) & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckWritePreconditions();

    if (const int count = GetCdbInt24(2); count) {
        for (int i = 0; i < count; i++) {
            WriteMetaData(object_type::filemark);
        }
    }

    StatusPhase();
}

void Tape::Locate(bool locate16)
{
    // CP is not supported
    if (GetCdbByte(1) & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint64_t identifier = locate16 ? GetCdbInt64(4) : GetCdbInt32(3);
    const bool bt = GetCdbByte(1) & 0x04;

    if (tar_file) {
        if (bt) {
            // The device-specific identifier must be a multiple of the block size
            if (identifier % GetBlockSize()) {
                throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
            }

            position = identifier;
            block_location = identifier / GetBlockSize();
        }
        else {
            position = identifier * GetBlockSize();
            block_location = identifier;
        }
    }
    else {
        if (bt && identifier) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
        } else {
            ResetPosition();
            FindNextObject(object_type::block, identifier);
        }
    }

    StatusPhase();
}

void Tape::ReadPosition() const
{
    vector<uint8_t> &buf = GetController()->GetBuffer();
    fill_n(buf.begin(), 20, 0);

    // BOP
    if (!position) {
        buf[0] = 0b10000000;
    }

    // EOP
    if (position >= file_size) {
        buf[0] += 0b01000000;
    }

    // Partition number is always 0

    // BT (SCSI-2)/service action 01 (since SSC-2)
    const bool bt = GetCdbByte(1) & 0x01;
    SetInt32(buf, 4, static_cast<uint32_t>(bt ? position : block_location));
    SetInt32(buf, 8, static_cast<uint32_t>(bt ? position : block_location));

    DataInPhase(20);
}

void Tape::FormatMedium()
{
    if (tar_file) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    CheckWritePreconditions();

    if (position) {
        throw scsi_exception(sense_key::illegal_request, asc::sequential_positioning_error);
    }

    Erase();

    ResetPosition();

    WriteMetaData(object_type::end_of_data);

    StatusPhase();
}

void Tape::WriteMetaData(Tape::object_type type, uint32_t size)
{
    if (tar_file) {
        return;
    }

    assert(!(size & 0xf0000000));

    file.seekp(position);

    switch (type) {
    case object_type::block:
    case object_type::filemark:
        position += WriteSimhMetaData(simh_class::tape_mark_good_data_record, size);
        break;

    case object_type::end_of_data:
        // Handled below
        break;

    default:
        // Write tape object types not supported by SIMH (e.g. end-of-data) as private markers
        WriteSimhMetaData(simh_class::private_marker, (static_cast<uint32_t>(type) << 24) | PRIVATE_MARKER_MAGIC);
        break;
    }

    // Ensure that there is always an end-of-data object after the last position
    WriteSimhMetaData(simh_class::private_marker,
        (static_cast<uint32_t>(object_type::end_of_data) << 24) | PRIVATE_MARKER_MAGIC);

    CheckForWriteError();
}

uint32_t Tape::FindNextObject(object_type type, int64_t count)
{
    const bool reverse = count < 0;

    LogTrace(fmt::format("{0}-spacing for object type {1}, count {2}", reverse ? "Reverse" : "Forward",
        static_cast<int>(type), count));

    if (reverse) {
        count = -count;
    }

    int64_t actual_count = 0;

    while (true) {
        if (reverse) {
            position -= MoveBackwards(count);
        }

        SimhMetaData meta_data = { };
        const auto [scsi_type, length] = ReadSimhMetaData(meta_data, reverse, true);

        LogTrace(fmt::format("Found object type {0} at position {1}, length {2}, spaced over {3} object(s)",
            static_cast<int>(scsi_type), position - META_DATA_SIZE, length, actual_count));

        if (type == scsi_type && count <= 1) {
            return AdjustResult(meta_data, reverse, type);
        }

        --count;

        // End-of-partition
        if (scsi_type == object_type::end_of_partition) {
            LogTrace(fmt::format("Encountered end-of-partition at position {} while spacing", position));
            SetInformation(count - actual_count + 1);
            SetEom(ascq::end_of_partition_medium_detected);
            throw scsi_exception(sense_key::medium_error, asc::no_additional_sense_information);
        }

        // End-of-data while spacing over blocks or filemarks
        if (scsi_type == object_type::end_of_data && type != object_type::end_of_data) {
            position -= META_DATA_SIZE;

            LogTrace(fmt::format("Encountered end-of-data at position {0} while spacing over object type{1}", position,
                static_cast<int>(type)));
            SetInformation(count - actual_count + 1);
            SetEom(ascq::end_of_data_detected);
            throw scsi_exception(sense_key::blank_check, asc::no_additional_sense_information);
        }

        // Terminate while spacing over blocks and a filemark is found
        if (scsi_type == object_type::filemark && type == object_type::block) {
            if (reverse) {
                position -= META_DATA_SIZE;
            }

            LogTrace(
                fmt::format("Encountered filemark at position {} while spacing over blocks",
                    reverse ? position : position - META_DATA_SIZE));
            SetInformation(actual_count);
            SetFilemark();
            throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
        }

        ++actual_count;

        AdjustForSpacing(meta_data, reverse);
    }
}

void Tape::ResetPosition()
{
    position = 0;
    block_location = 0;
}

void Tape::AdjustForSpacing(const SimhMetaData &meta_data, bool reverse)
{
    if (reverse) {
        position -= META_DATA_SIZE;
    }
    else if (IsRecord(meta_data)) {
        position += meta_data.value + GetPadding(meta_data.value) + META_DATA_SIZE;
    }

    if (IsRecord(meta_data) || (meta_data.cls == simh_class::bad_data_record && !meta_data.value)) {
        if (reverse) {
            --block_location;
        }
        else {
            ++block_location;
        }
    }
}

void Tape::AdjustForReading(const SimhMetaData &meta_data, bool reverse)
{
    if (reverse) {
        position -= 2 * META_DATA_SIZE;
    }

    if (IsRecord(meta_data)) {
        if (reverse) {
            --block_location;
        }
        else {
            position += meta_data.value + GetPadding(meta_data.value) + META_DATA_SIZE;
            ++block_location;
        }
    }
}

uint32_t Tape::AdjustResult(const SimhMetaData &meta_data, bool reverse, object_type type)
{
    if (reverse) {
        if (type == object_type::block) {
            AdjustForSpacing(meta_data, true);
        }
        else {
            position -= META_DATA_SIZE;
        }
    }
    else {
        AdjustForSpacing(meta_data, false);
    }

    return meta_data.value;
}

int64_t Tape::MoveBackwards(int64_t count)
{
    if (position < META_DATA_SIZE) {
        LogTrace("Encountered beginning-of-partition while reverse-spacing");
        ResetPosition();
        SetInformation(count);
        SetEom(ascq::beginning_of_partition_medium_detected);
        throw scsi_exception(sense_key::no_sense);
    }

    // Position before trailing length or marker
    position -= META_DATA_SIZE;

    file.seekg(position, ios::beg);

    SimhMetaData meta_data;
    if (!ReadMetaData(file, meta_data)) {
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }

    return IsRecord(meta_data) ? meta_data.value + GetPadding(meta_data.value) + META_DATA_SIZE : 0;
}

uint32_t Tape::GetByteCount()
{
    fixed = GetCdbByte(1) & 0x01;
    const int32_t count = fixed ?
            GetSignedInt24(GetController()->GetCdb(), 2) * GetBlockSize() :
            GetSignedInt24(GetController()->GetCdb(), 2);

    LogTrace(fmt::format("Current position: {0}, requested byte count: {1}", position, count));

    return count;
}

void Tape::Erase()
{
    file.seekp(position);

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

    uint64_t remaining = file_size - position;
    while (remaining >= 4) {
        uint64_t chunk = remaining;
        if (chunk > buf.size()) {
            chunk = buf.size();
        }

        file.write((const char*)buf.data(), chunk);
        CheckForWriteError();

        remaining -= chunk;
        position += chunk;
        block_location += chunk / GetBlockSize();
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

pair<Tape::object_type, int> Tape::ReadSimhMetaData(SimhMetaData &meta_data, bool reverse, bool find_only)
{
    while (true) {
        file.seekg(position, ios::beg);

        if (!ReadMetaData(file, meta_data)) {
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        LogTrace(fmt::format("Read SIMH meta data with class {0:1X}, value ${1:07x} at position {2}",
            static_cast<int>(meta_data.cls), meta_data.value, position));

        position += META_DATA_SIZE;

        switch (meta_data.cls) {
        case simh_class::tape_mark_good_data_record:
            return {meta_data.value ? object_type::block : object_type::filemark, meta_data.value};

        case simh_class::bad_data_record:
            // Consider recovered bad data records valid
            if (meta_data.value || find_only) {
                return {object_type::block, meta_data.value};
            }
            throw scsi_exception(sense_key::medium_error, asc::read_error);

        case simh_class::reserved_marker:
            if (meta_data.value == static_cast<int>(simh_marker::end_of_medium)) {
                return {object_type::end_of_partition, 0};
            }
            LogTrace(
                meta_data.value == static_cast<int>(simh_marker::erase_gap) ?
                    "Skipping SIMH erase gap" : "Skipping unknown SIMH reserved marker");
            AdjustForReading(meta_data, reverse);
            break;

        case simh_class::private_marker:
            if ((meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC) {
                LogTrace(fmt::format("Found SCSI2Pi private marker for object type {0} at position {1}",
                    (meta_data.value >> 24) & 0x0f, position - META_DATA_SIZE));
                AdjustForReading(meta_data, reverse);
                return {static_cast<object_type>((meta_data.value >> 24) & 0x0f), 0};
            }
            LogTrace(fmt::format("Skipping unknown SIMH private marker, value ${0:07x} at position {1}",
                static_cast<int>(meta_data.value), position - META_DATA_SIZE));
            AdjustForReading(meta_data, reverse);
            break;

        default:
            LogTrace(fmt::format("Skipping unknown SIMH class {0:1X} at position {1}",
                static_cast<int>(meta_data.cls), position - META_DATA_SIZE));
            AdjustForReading(meta_data, reverse);
            break;
        }
    }
}

int Tape::WriteSimhMetaData(simh_class cls, uint32_t value)
{
    LogTrace(
        fmt::format("Writing SIMH meta_data with class {0:1X}, value ${1:07x} to position {2}", static_cast<int>(cls),
        value, position));

    if (position + META_DATA_SIZE > max_file_size) {
        ++write_error_count;
        throw scsi_exception(sense_key::volume_overflow);
    }

    file.write((const char*)ToLittleEndian( { cls, value }).data(), META_DATA_SIZE);
    CheckForWriteError();

    return META_DATA_SIZE;
}

void Tape::CheckBlockLength(int length)
{
    if (static_cast<int>(record_length) != length) {
        // In fixed mode an incorrect length always results in an error.
        // SSC-5: "If the FIXED bit is one, the INFORMATION field shall be set to the requested transfer length
        // minus the actual number of logical blocks read, not including the incorrect-length logical block."
        if (fixed) {
            SetIli();
            SetInformation((remaining_count - byte_count) / GetBlockSize() - blocks_read);
            throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
        }
        // Report CHECK CONDITION if SILI is not set and the actual length does not match the requested length.
        // SSC-5: "If the FIXED bit is zero, the INFORMATION field shall be set to the requested transfer length
        // minus the actual logical block length."
        // If SILI is set report CHECK CONDITION for the overlength condition only.
        else if ((!(GetCdbByte(1) & 0x02) && record_length % GetBlockSize())
            || length > static_cast<int>(record_length)) {
            SetIli();
            SetInformation(length - record_length);
            throw scsi_exception(sense_key::no_sense, asc::no_additional_sense_information);
        }
    }
}

bool Tape::IsAtRecordBoundary() const
{
    if (tar_file) {
        return false;
    }

    return fixed || !record_length || !remaining_count || byte_count - remaining_count == record_length;
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
