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
#include "shared/s2p_exceptions.h"
#include "shared/s2p_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Tape::Tape(int lun) : StorageDevice(SCTP, scsi_level::scsi_2, lun, true, false, { 256, 512, 1024, 2048, 4096, 8192 })
{
    SetProduct("SCSI TAPE");
    SetProtectable(true);
    SetRemovable(true);
}

bool Tape::SetUp()
{
    AddCommand(scsi_command::read6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::write6, [this]
        {
            Write6();
        });
    AddCommand(scsi_command::erase6, [this]
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
    AddCommand(scsi_command::space6, [this]
        {
            Space6();
        });
    AddCommand(scsi_command::write_filemarks6, [this]
        {
            WriteFilemarks6();
        });
    AddCommand(scsi_command::locate10, [this]
        {
            Locate(false);
        });
    AddCommand(scsi_command::locate16, [this]
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

    tar_mode = GetExtensionLowerCase(GetFilename()) == "tar";
}

void Tape::Read6()
{
    // FIXED and SILI must not both be set
    if ((GetCdbByte(1) & 0x03) == 0x03) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int count = GetByteCount();
    if (count) {
        blocks_read = 0;

        GetController()->SetTransferSize(count, count % GetBlockSize() ? count : GetBlockSize());

        GetController()->SetCurrentLength(count);
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
        GetController()->SetTransferSize(byte_count, byte_count % GetBlockSize() ? byte_count : GetBlockSize());

        DataOutPhase(byte_count % GetBlockSize() ? byte_count : GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

int Tape::GetVariableBlockSize()
{
    const int length = FindNextObject(object_type::block, 0);

    // Check for incorrect block length
    if (length != GetController()->GetChunkSize()) {
        LogTrace(fmt::format("Actual block length of {0} byte(s) does not match expected length of {1} byte(s)",
            length, GetController()->GetChunkSize()));

        const auto requested_length = GetSignedInt24(GetController()->GetCdb(), 2);

        // In fixed mode an incorrect length always results in an error.
        // SSC-5: "If the FIXED bit is one, the INFORMATION field shall be set to the requested transfer length
        // minus the actual number of logical blocks read, not including the incorrect-length logical block."
        if (GetCdbByte(1) & 0x01) {
            SetIli();
            SetInformation(requested_length - blocks_read);
            throw scsi_exception(sense_key::medium_error, asc::no_additional_sense_information);
        }

        // In non-fixed mode the error handling depends on SILI.
        // Report CHECK CONDITION if SILI is not set and the actual length does not match the requested length.
        // SSC-5: "If the FIXED bit is zero, the INFORMATION field shall be set to the requested transfer length
        // minus the actual logical block length."
        // If SILI is set report CHECK CONDITION for the overlength condition only.
        if (!(GetCdbByte(1) & 0x02) || length > requested_length) {
            SetIli();
            SetInformation(requested_length - length);
            throw scsi_exception(sense_key::medium_error, asc::no_additional_sense_information);
        }
    }

    return length;
}

int Tape::ReadData(span<uint8_t> buf)
{
    CheckReady();

    const int size = tar_mode ? GetBlockSize() : GetVariableBlockSize();

    LogTrace(fmt::format("Reading {0} data byte(s) from position {1}", size, position));

    file.seekg(position, ios::beg);

    // TODO Move all tar-related code to separate namespace or class, just like the simh code
    int length = READ_ERROR;
    if (tar_mode) {
        file.read((char*)buf.data(), size);
        if (file.fail()) {
            file.clear();
        }
        else {
            length = size;
        }
    }
    else {
        length = ReadRecord( { file, file_size }, buf, size);
        if (length >= 0) {
            length = Pad(length);
        }
    }

    if (length < 0) {
        ++read_error_count;
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }

    position += length;
    ++block_location;
    ++blocks_read;

    return size;
}

int Tape::WriteData(span<const uint8_t> buf, scsi_command)
{
    CheckReady();

    const uint32_t size = GetController()->GetChunkSize();

    WriteMetaData(object_type::block, size);

    LogTrace(fmt::format("Writing {0} data byte(s) to position {1}", size, position));

    file.seekp(position, ios::beg);

    if (tar_mode) {
        if (position + size > file_size) {
            throw scsi_exception(sense_key::volume_overflow);
        }

        file.write((const char*)buf.data(), size);

        position += size;
    }
    else {
        const int l = WriteRecord( { file, file_size }, buf, size);
        if (l < 0) {
            ++write_error_count;
            throw scsi_exception(sense_key::volume_overflow);
        }

        position += l;

        WriteMetaData(object_type::end_of_data);
    }

    file.flush();
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    byte_count -= size;
    ++block_location;

    return size;
}

void Tape::Open()
{
    assert(!IsReady());

    // Block size and number of blocks
    if (!SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 512)) {
        throw io_exception("Invalid block size");
    }

    file_size = GetFileSize();

    SetBlockCount(static_cast<uint32_t>(file_size / GetBlockSize()));

    ValidateFile();

    ResetPosition();

}

bool Tape::Eject(bool force)
{
    bool status = StorageDevice::Eject(force);
    if (status) {
        file.close();

        read_error_count = 0;
        write_error_count = 0;
    }

    return status;
}

vector<uint8_t> Tape::InquiryInternal() const
{
    return HandleInquiry(device_type::sequential_access, true);
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
    if (tar_mode) {
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

    file.flush();
    if (file.fail()) {
        file.clear();
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

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
    file.seekg(0, ios::beg);
    file.seekp(0, ios::beg);

    ResetPosition();

    StatusPhase();
}

void Tape::Space6()
{
    if (tar_mode) {
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
    if (tar_mode) {
        LogInfo("Writing filemarks in tar-compatibility mode is not supported, WRITE FILEMARKS(6) command is ignored");
        StatusPhase();
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

        if (position + HEADER_SIZE <= file_size) {
            WriteMetaData(object_type::end_of_data);
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

    if (tar_mode) {
        position = identifier * GetBlockSize();
        block_location = identifier;
    }
    else {
        ResetPosition();

        // BT
        if (GetCdbByte(1) & 0x01) {
            position = identifier;
            block_location = position / GetBlockSize();
        } else {
            FindNextObject(object_type::filemark, identifier);
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
    if (tar_mode) {
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
    if (tar_mode) {
        return;
    }

    assert(!(size & 0xf0000000));

    file.seekp(position, ios::beg);

    switch (type) {
    case object_type::block:
    case object_type::filemark:
        position += WriteSimhHeader(simh_class::tape_mark_good_data_record, size);
        break;

    case object_type::end_of_data:
        WriteSimhHeader(simh_class::reserved_marker, static_cast<int>(simh_marker::end_of_medium));
        break;

    default:
        assert(false);
        break;
    }
}

uint32_t Tape::FindNextObject(Tape::object_type type, int64_t count)
{
    const bool reverse = count < 0;
    if (reverse) {
        count = -count;
    }

    while (true) {
        if (reverse) {
            // SSC-5: "If the device server reaches beginning-of-partition before positioning over N logical objects,
            // then it shall end movement at beginning-of-partition."
            if (position < HEADER_SIZE) {
                LogTrace("Encountered beginning-of-partition while spacing");
                ResetPosition();
                return 0;
            }

            file.seekg(position, ios::beg);

            position = MoveBack(file);
            if (position < 0) {
                throw scsi_exception(sense_key::medium_error, asc::read_error);
            }
        }

        const auto old_position = position;

        const auto [scsi_type, length] = ReadSimhHeader();

        LogTrace(fmt::format("Searching for object type {0}, found type {1} at position {2}", static_cast<int>(type),
            static_cast<int>(scsi_type), old_position));

        if (type == scsi_type) {
            if (!count) {
                return static_cast<uint32_t>(length);
            }

            --count;
        }

        // End-of-partition
        if (scsi_type == object_type::end_of_partition) {
            LogTrace("Encountered end-of-partition while spacing");
            SetInformation(count);
            SetEom();
            throw scsi_exception(sense_key::medium_error);
        }

        // End-of-data while spacing over blocks or filemarks
        if (scsi_type == object_type::end_of_data && (type != object_type::end_of_data)) {
            LogTrace(fmt::format("Encountered end-of-data while spacing over {}",
                type == object_type::block ? "blocks" : "filemarks"));
            SetInformation(count);
            throw scsi_exception(sense_key::blank_check);
        }

        // Terminate while spacing over blocks and a filemark is found
        if (scsi_type == object_type::filemark && type == object_type::block) {
            LogTrace("Encountered filemark while spacing over blocks");
            SetInformation(count);
            SetFilemark();
            throw scsi_exception(sense_key::no_sense);
        }

        if (scsi_type == object_type::block) {
            if (reverse) {
                --block_location;
            }
            else {
                position += length + HEADER_SIZE;
                ++block_location;
            }
        }
    }
}

uint32_t Tape::GetByteCount() const
{
    const bool fixed = GetCdbByte(1) & 0x01;
    const int32_t count = fixed ?
            GetSignedInt24(GetController()->GetCdb(), 2) * GetBlockSize() :
            GetSignedInt24(GetController()->GetCdb(), 2);

    // SSC-5: The non-fixed block size must be a multiple of 4
    if (!fixed && count % 4) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    LogTrace(fmt::format("Current position: {0}, requested byte count: {1}", position, count));

    return count;
}

void Tape::Erase()
{
    file.seekp(position, ios::beg);

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
        if (file.fail()) {
            file.clear();
            throw scsi_exception(sense_key::medium_error, asc::write_error);
        }

        remaining -= chunk;
        position += chunk;
        block_location += chunk / GetBlockSize();
    }

    file.flush();
    if (file.fail()) {
        file.clear();
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }
}

void Tape::ResetPosition()
{
    position = 0;
    block_location = 0;
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

pair<Tape::object_type, int> Tape::ReadSimhHeader()
{
    while (true) {
        const auto old_position = position;

        file.seekg(position, ios::beg);

        SimhHeader header;
        position += ReadHeader( { file, file_size }, header);
        if (header.cls == simh_class::reserved_marker && header.value == static_cast<int>(simh_marker::end_of_medium)) {
            return {object_type::end_of_partition, 0};
        }

        LogTrace(fmt::format("Read SIMH header with class {0:1X}, value ${1:07x} at position {2}",
            static_cast<int>(header.cls), header.value, old_position));

        switch (header.cls) {
        case simh_class::tape_mark_good_data_record:
            return {header.value ? object_type::block : object_type::filemark, header.value};

        case simh_class::bad_data_record:
        case simh_class::error:
            throw scsi_exception(sense_key::medium_error, asc::read_error);

        case simh_class::reserved_marker:
            if (header.value == static_cast<int>(simh_marker::end_of_medium)) {
                return {object_type::end_of_data, 0};
            }

            LogTrace(
                header.value == static_cast<int>(simh_marker::erase_gap) ?
                    "Skipping erase gap" : "Skipping unknown SIMH reserved marker");
            break;

        default:
            LogTrace("Skipping unknown SIMH class");
            if (IsRecord(header.cls)) {
                position += header.value;
            }
            break;
        }
    }

    assert(false);
}

int Tape::WriteSimhHeader(simh_class cls, uint32_t value)
{
    LogTrace(fmt::format("Writing SIMH header with class {0:1X}, value ${1:07x} to position {2}", static_cast<int>(cls),
        value, static_cast<uint64_t>(file.tellp())));

    const int size = WriteHeader( { file, file_size }, { cls, value });
    switch (size) {
    case OVERFLOW_ERROR:
        ++write_error_count;
        throw scsi_exception(sense_key::volume_overflow);

    case WRITE_ERROR:
        ++write_error_count;
        throw scsi_exception(sense_key::medium_error, asc::write_error);

    default:
        break;
    }

    return size;
}
