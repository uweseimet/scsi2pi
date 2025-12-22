//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
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
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Tape::Tape(int l) : StorageDevice(SCTP, l, true, false, { 512, 1024, 2048, 4096, 8192 })
{
    SetProductData( { "", "SCSI TAPE", "" }, true);
    SetScsiLevel(ScsiLevel::SCSI_2);
    SupportsParams(true);
    SetProtectable(true);
    SetRemovable(true);
}

string Tape::SetUp()
{
    AddCommand(ScsiCommand::READ_6, [this]
        {
            Read(false);
        });
    AddCommand(ScsiCommand::READ_16, [this]
        {
            Read(true);
        });
    AddCommand(ScsiCommand::WRITE_6, [this]
        {
            Write(false);
        });
    AddCommand(ScsiCommand::WRITE_16, [this]
        {
            Write(true);
        });
    AddCommand(ScsiCommand::ERASE_6, [this]
        {
            Erase6();
        });
    AddCommand(ScsiCommand::READ_BLOCK_LIMITS, [this]
        {
            ReadBlockLimits();
        });
    AddCommand(ScsiCommand::REWIND, [this]
        {
            CheckReady();
            ResetPositions();
            StatusPhase();
        });
    AddCommand(ScsiCommand::SPACE_6, [this]
        {
            Space6();
        });
    AddCommand(ScsiCommand::WRITE_FILEMARKS_6, [this]
        {
            WriteFilemarks(false);
        });
    AddCommand(ScsiCommand::WRITE_FILEMARKS_16, [this]
        {
            WriteFilemarks(true);
        });
    AddCommand(ScsiCommand::LOCATE_10, [this]
        {
            Locate(false);
            StatusPhase();
        });
    AddCommand(ScsiCommand::LOCATE_16, [this]
        {
            Locate(true);
            StatusPhase();
        });
    AddCommand(ScsiCommand::READ_POSITION, [this]
        {
            ReadPosition();
        });
    AddCommand(ScsiCommand::FORMAT_MEDIUM, [this]
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
        throw IoException("Can't open image file '" + GetFilename() + "'");
    }

    tar_file = GetExtensionLowerCase(GetFilename()) == "tar";

    if (IsReady()) {
        SetAttn(true);
    }
}

void Tape::Read(bool read_16)
{
    CheckReady();

    // FIXED and SILI must not both be set, only partition 0 is supported
    if ((GetCdbByte(1) & 0b11) == 0b11 || (read_16 && GetCdbByte(3))) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    expl = read_16;
    if (expl && !Locate(true)) {
        SetInformation(GetCdbInt24(12));
        throw ScsiException(SenseKey::NO_SENSE, Asc::LOCATE_OPERATION_FAILURE);
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
    CheckReady();

    // FCS and LCS are not supported, only partition 0 is supported
    if (write_16 && (GetCdbByte(1) & 0b1100 || GetCdbByte(3))) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    CheckWritePreconditions();

    expl = write_16;
    if (expl && !Locate(true)) {
        SetInformation(GetCdbInt24(12));
        throw ScsiException(SenseKey::NO_SENSE, Asc::LOCATE_OPERATION_FAILURE);
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
    int length = GetController()->GetChunkSize();

    if (IsAtRecordBoundary()) {
        file.seekg(tape_position);

        current_meta_data = FindNextObject(ObjectType::BLOCK, 0, true);

        record_length = current_meta_data.value;

        if (current_meta_data.cls == SimhClass::BAD_DATA_RECORD && !record_length) {
            ++read_error_count;
            throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
        }

        tape_position -= record_length + META_DATA_SIZE;

        record_start = tape_position;

        const uint32_t actual_length = CheckBlockLength();
        if (actual_length) {
            byte_count = actual_length < byte_count ? actual_length : byte_count;
            remaining_count = byte_count;
            length = static_cast<int>(actual_length) < length ? static_cast<int>(actual_length) : length;

            GetController()->SetTransferSize(byte_count, GetBlockSize());
            GetController()->SetCurrentLength(GetBlockSize());
        }
    }

    LogTrace(
        fmt::format("Reading {0} data byte(s) from position {1}, record length is {2}", length, tape_position, record_length));

    file.seekg(tape_position);
    file.read((char*)buf.data(), length);
    CheckForReadError();

    remaining_count -= length;
    tape_position += length;

    if (IsAtRecordBoundary()) {
        tape_position = record_start + Pad(record_length);
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
    if (IsAtRecordBoundary()) {
        WriteMetaData(ObjectType::BLOCK, record_length);
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

        tape_position += WriteSimhMetaData(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, record_length);

        ++object_location;
    }

    if (!remaining_count) {
        // Ensure that there is always an end-of-data object
        WriteMetaData(ObjectType::END_OF_DATA);
    }

    return chunk_size;
}

void Tape::Open()
{
    assert(!IsReady());

    if (const int append = ParseAsUnsignedInt(GetParam(APPEND)); append == -1) {
        throw ParserException(fmt::format("Invalid maximum file size: '{}'", GetParam(APPEND)));
    }
    else {
        max_file_size = append;
    }

    // This call cannot fail, the method argument is always valid
    SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 512);

    if (max_file_size && max_file_size < GetBlockSize()) {
        throw IoException(
            fmt::format("Maximum file size {0} is smaller than block size {1}", max_file_size, GetBlockSize()));
    }

    block_size_for_descriptor = GetBlockSize();

    try {
        file_size = GetFileSize();
    }
    catch (const IoException&) {
        file_size = 0;
    }

    // In append mode, ensure that the image file size is at least the block size
    if (max_file_size && file_size < GetBlockSize()) {
        file.open(GetFilename(), ios::out | ios::binary);
        file.seekp(GetBlockSize() - 1);
        file.put(0);
        file.flush();
        if (file.bad()) {
            file.close();
            throw IoException(fmt::format("Can't write to '{}'", GetFilename()));
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
    return HandleInquiry(DeviceType::SEQUENTIAL_ACCESS, true);
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

void Tape::AddDataCompressionPage(map<int, vector<byte>> &pages)
{
    vector<byte> buf(16);

    pages[15] = buf;
}

void Tape::AddDeviceConfigurationPage(map<int, vector<byte>> &pages, bool changeable)
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

void Tape::AddMediumPartitionPage(map<int, vector<byte> > &pages, bool changeable)
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
    CheckReady();

    if (tar_file) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
    }

    CheckWritePreconditions();

    // Check Long bit. Like with an HP35470A a long erase erases everything, otherwise only EOD is written.
    if (GetCdbByte(1) & 0x01) {
        Erase();

        // After a Long erase according to the standard the position is undefined, for SCSI2Pi it is 0
        ResetPositions();
    }

    WriteMetaData(ObjectType::END_OF_DATA);

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

void Tape::Space6()
{
    CheckReady();

    if (tar_file) {
        LogError("In tar-compatibility mode spacing is not supported");
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
    }

    switch (const auto code = static_cast<ObjectType>(GetCdbByte(1) & 0x07); code) {
    case ObjectType::BLOCK:
    case ObjectType::FILEMARK:
        {
        const int count = GetCdbInt24(2);
        if (count) {
            // The count is signed
            FindNextObject(code, count >= 0x800000 ? count - 0x1000000 : count, false);
        }
    }
        break;

    case ObjectType::END_OF_DATA:
        FindNextObject(ObjectType::END_OF_DATA, 0, false);
        tape_position -= META_DATA_SIZE;
        break;

    default:
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    StatusPhase();
}

void Tape::WriteFilemarks(bool write_filemarks_16)
{
    CheckReady();

    if (tar_file) {
        LogTrace("In tar-compatibility mode writing filemarks is ignored");
        StatusPhase();
        return;
    }

    // Since SSC-3 setmarks are not supported anymore, FCS/LCS are not supported, only partition 0 is supported
    if (GetCdbByte(1) & 0x02 || (write_filemarks_16 && (GetCdbByte(1) & 0b1100 || GetCdbByte(3)))) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    CheckWritePreconditions();

    int count;
    if (write_filemarks_16) {
        if (const auto identifier = static_cast<uint32_t>(GetCdbInt64(4)); identifier) {
            ResetPositions();
            if (!FindObject(identifier)) {
                SetInformation(GetCdbInt24(12));
                throw ScsiException(SenseKey::NO_SENSE, Asc::LOCATE_OPERATION_FAILURE);
            }
        }

        count = GetCdbInt24(12);
    }
    else {
        count = GetCdbInt24(2);
    }

    for (int i = 0; i < count; ++i) {
        WriteMetaData(ObjectType::FILEMARK);
    }

    StatusPhase();
}

bool Tape::Locate(bool locate_16)
{
    CheckReady();

    // CP is not supported
    if (GetCdbByte(1) & 0x02) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    auto identifier = locate_16 ? GetCdbInt64(4) : GetCdbInt32(3);
    const bool bt = GetCdbByte(1) & 0x04;

    if (tar_file) {
        if (bt) {
            // The device-specific identifier must be a multiple of the block size
            if (identifier % GetBlockSize()) {
                throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
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
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
        } else {
            ResetPositions();
            if (identifier) {
                return FindObject(static_cast<int>(identifier));
            }
        }
    }

    return true;
}

void Tape::ReadPosition()
{
    CheckReady();

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
    CheckReady();

    if (tar_file) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
    }

    CheckWritePreconditions();

    if (tape_position) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::SEQUENTIAL_POSITIONING_ERROR);
    }

    Erase();

    ResetPositions();

    WriteMetaData(ObjectType::END_OF_DATA);

    StatusPhase();
}

void Tape::WriteMetaData(Tape::ObjectType object_type, uint32_t size)
{
    if (tar_file) {
        return;
    }

    assert(!(size & 0xf0000000));

    file.seekp(tape_position);

    if (object_type == ObjectType::BLOCK || object_type == ObjectType::FILEMARK) {
        tape_position += WriteSimhMetaData(SimhClass::TAPE_MARK_GOOD_DATA_RECORD, size);
    }

    // Ensure that there is always an end-of-data object behind the last position
    if (file_size >= tape_position + META_DATA_SIZE) {
        WriteSimhMetaData(SimhClass::PRIVATE_MARKER,
            (static_cast<uint32_t>(ObjectType::END_OF_DATA) << 24) | PRIVATE_MARKER_MAGIC);
    }

    CheckForWriteError();
}

SimhMetaData Tape::FindNextObject(ObjectType type_to_find, int32_t requested_count, bool read)
{
    LogTrace(fmt::format("Searching for object type {0} with count {1} at position {2}", static_cast<int>(type_to_find),
        requested_count, tape_position));

    const bool reverse = requested_count < 0;
    if (reverse) {
        requested_count = -requested_count;
    }

    int32_t actual_count = 0;

    while (true) {
        SimhMetaData meta_data;
        const auto [type_found, length] = ReadSimhMetaData(meta_data, requested_count, reverse);

        // Bad data (not recovered) during READ(6)
        if (read && type_found == ObjectType::BLOCK && !length) {
            return meta_data;
        }

        LogTrace(
            fmt::format("Found object type {0}, length {1}, moved over {2} object(s)", static_cast<int>(type_found),
                length, actual_count));

        if (!reverse && IsRecord(meta_data)) {
            tape_position += Pad(meta_data.value) + META_DATA_SIZE;
        }

        if (type_found == ObjectType::END_OF_DATA) {
            if (type_to_find == ObjectType::END_OF_DATA) {
                return meta_data;
            }

            // End-of-data while spacing over something else
            RaiseEndOfData(type_to_find, requested_count);
        }
        else if (type_found == ObjectType::FILEMARK && type_to_find == ObjectType::BLOCK) {
            // Terminate while spacing over blocks and a filemark is found
            RaiseFilemark(requested_count, reverse, read);
        }

        // For end-of-data the count is ignored
        if (type_to_find != ObjectType::END_OF_DATA && type_found == type_to_find) {
            --requested_count;
            if (requested_count <= 0) {
                return meta_data;
            }

            ++actual_count;
        }
    }
}

void Tape::RaiseBeginningOfPartition(int32_t info)
{
    ResetPositions();

    SetInformation(info);
    SetEom(Ascq::BEGINNING_OF_PARTITION_MEDIUM_DETECTED);

    throw ScsiException(SenseKey::NO_SENSE);
}

void Tape::RaiseEndOfPartition(int32_t info)
{
    SetInformation(info);
    SetEom(Ascq::END_OF_PARTITION_MEDIUM_DETECTED);

    throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
}

void Tape::RaiseEndOfData(ObjectType object_type, int32_t info)
{
    tape_position -= META_DATA_SIZE;

    LogTrace(fmt::format("Encountered end-of-data at position {0} while spacing over object type {1}", tape_position,
        static_cast<int>(object_type)));

    SetInformation(info);

    throw ScsiException(SenseKey::BLANK_CHECK, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
}

void Tape::RaiseFilemark(int32_t info, bool reverse, bool read)
{
    LogTrace(fmt::format("Encountered filemark at position {} while spacing over blocks",
        reverse ? tape_position : tape_position - META_DATA_SIZE));

    if (read && !fixed) {
        SetInformation(GetByteCount());
    }
    else {
        SetInformation(reverse ? -info : info);
    }
    SetFilemark();

    throw ScsiException(SenseKey::NO_SENSE, Asc::NO_ADDITIONAL_SENSE_INFORMATION);
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

    throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
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
            throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
        }
        tape_position -= IsRecord(meta_data) ? Pad(meta_data.value) + META_DATA_SIZE : 0;
    }
    else {
        file.seekg(tape_position);
        if (!ReadMetaData(file, meta_data)) {
            ++read_error_count;
            throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
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

        if (meta_data.cls == SimhClass::PRIVATE_MARKER && (meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC) {
            return false;
        }

        if (IsRecord(meta_data) || (meta_data.cls == SimhClass::BAD_DATA_RECORD && !meta_data.value)
            || (meta_data.cls == SimhClass::TAPE_MARK_GOOD_DATA_RECORD && !meta_data.value)) {
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
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const int length = GetCdbInt24(expl ? 12 : 2);
    const int32_t count = fixed ? length * GetBlockSize() : length;

    LogTrace(fmt::format("Current position: {0}, requested byte count: {1}", tape_position, count));

    return count;
}

void Tape::Erase()
{
    file.seekp(tape_position);

    // Erase in chunks, using SIMH gaps as a pattern (little endian)
    vector<uint8_t> buf(1024 * sizeof(SimhMarker::ERASE_GAP));
    const auto gap = static_cast<uint32_t>(SimhMarker::ERASE_GAP);
    for (size_t i = 0; i < buf.size(); i += 4) {
        buf[i] = gap & 0xff;
        buf[i + 1] = (gap >> 8) & 0xff;
        buf[i + 2] = (gap >> 16) & 0xff;
        buf[i + 3] = (gap >> 24) & 0xff;
    }

    uint64_t remaining = file_size - tape_position;
    while (remaining >= 4) {
        const uint64_t chunk = min(remaining, static_cast<uint64_t>(buf.size())); // NOSONAR Cast is required for armv6

        file.write((const char*)buf.data(), chunk);
        CheckForWriteError();

        remaining -= chunk;
        tape_position += chunk;
        object_location += chunk / GetBlockSize();
    }
}

pair<Tape::ObjectType, int> Tape::ReadSimhMetaData(SimhMetaData &meta_data, int32_t count, bool reverse)
{
    while (ReadNextMetaData(meta_data, reverse)) {
        UpdateObjectLocation(meta_data, reverse);

        switch (meta_data.cls) {
        case SimhClass::TAPE_MARK_GOOD_DATA_RECORD:
            return {meta_data.value ? ObjectType::BLOCK : ObjectType::FILEMARK, meta_data.value};

        case SimhClass::BAD_DATA_RECORD:
            return {ObjectType::BLOCK, meta_data.value};

        case SimhClass::RESERVERD_MARKER:
            if (meta_data.value == static_cast<int>(SimhMarker::END_OF_MEDIUM)) {
                RaiseEndOfPartition(count);
            }
            LogTrace(
                meta_data.value == static_cast<int>(SimhMarker::ERASE_GAP) ?
                    "Skipping SIMH erase gap" : "Skipping unknown SIMH reserved marker");
            break;

        case SimhClass::PRIVATE_MARKER:
            if ((meta_data.value & 0x00ffffff) == PRIVATE_MARKER_MAGIC) {
                LogTrace(fmt::format("Found SCSI2Pi private marker for object type {0} at position {1}",
                    (meta_data.value >> 24) & 0x0f, tape_position - META_DATA_SIZE));
                return {static_cast<ObjectType>((meta_data.value >> 24) & 0x0f), 0};
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

    RaiseBeginningOfPartition(count);
}

void Tape::UpdateObjectLocation(const SimhMetaData &meta_data, bool reverse)
{
    if (IsRecord(meta_data) || (meta_data.cls == SimhClass::BAD_DATA_RECORD && !meta_data.value)
        || (meta_data.cls == SimhClass::TAPE_MARK_GOOD_DATA_RECORD && !meta_data.value)) {
        object_location += reverse ? -1 : 1;
    }
}

int Tape::WriteSimhMetaData(SimhClass cls, uint32_t value)
{
    LogTrace(
        fmt::format("Writing SIMH meta data with class {0:1X}, value ${1:07x} to position {2}", static_cast<int>(cls),
            value, tape_position));

    CheckForOverflow(tape_position + META_DATA_SIZE);

    file.write((const char*)ToLittleEndian( { cls, value }).data(), META_DATA_SIZE);
    CheckForWriteError();

    return META_DATA_SIZE;
}

uint32_t Tape::CheckBlockLength()
{
    if (record_length != byte_count) {
        // In fixed mode an incorrect length results in an error if it is not a multiple of the block size.
        // SSC-5: "If the FIXED bit is one, the INFORMATION field shall be set to the requested transfer length
        // minus the actual number of logical blocks read, not including the incorrect-length logical block."
        if (fixed && byte_count % record_length) {
            SetIli();
            SetInformation((byte_count - remaining_count) / GetBlockSize() - blocks_read);

            GetController()->SetStatus(StatusCode::CHECK_CONDITION);

            return min(record_length, byte_count);
        }

        // Report CHECK CONDITION if SILI is not set and the actual length does not match the requested length.
        // SSC-5: "If the FIXED bit is zero, the INFORMATION field shall be set to the requested transfer length
        // minus the actual logical block length."
        // If SILI is set report CHECK CONDITION for the overlength condition only.
        if (!fixed && (!(GetCdbByte(1) & 0x02) || byte_count > record_length)) {
            SetIli();
            SetInformation(byte_count - record_length);

            GetController()->SetStatus(StatusCode::CHECK_CONDITION);

            return min(record_length, byte_count);
        }
    }

    return 0;
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
        throw ScsiException(SenseKey::VOLUME_OVERFLOW);
    }
}

void Tape::CheckForReadError()
{
    if (file.fail()) {
        file.clear();
        ++read_error_count;
        throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
    }
}

void Tape::CheckForWriteError()
{
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::WRITE_ERROR);
    }

    file.flush();
}

vector<PbStatistics> Tape::GetStatistics() const
{
    vector<PbStatistics> statistics = StorageDevice::GetStatistics();

    EnrichStatistics(statistics, CATEGORY_ERROR, READ_ERROR_COUNT, read_error_count);
    if (!IsReadOnly()) {
        EnrichStatistics(statistics, CATEGORY_ERROR, WRITE_ERROR_COUNT, write_error_count);
    }

    return statistics;
}
