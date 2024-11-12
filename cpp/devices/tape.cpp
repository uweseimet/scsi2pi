//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// The SCTP device is a SCSI-2 sequential access device with some SSC-2 command extensions.
//
// This implementation supports using .tar files as tape image files. Filemark, end-of-data and spacing support
// is not possible with these files, because .tar files do not contain any meta data.
// Note that tar (actually the Linux tape device driver) tries to create a filemark as an end-of-data marker when
// writing to a tape device, but not when writing to a local .tar file.
// This implementation has successfully been tested with tar, mt and mtx on Linux and with Gemar on the Atari.
//
//---------------------------------------------------------------------------

#include "tape.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_util.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Tape::Tape(int lun) : StorageDevice(SCTP, scsi_level::scsi_2, lun, true, false, { 256, 512, 1024, 2048, 4096 })
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
    if (!file.good()) {
        throw io_exception("Can't open image file '" + GetFilename() + "'");
    }

    // The SCSI2Pi 4.0 implementation only supports tar mode
    tar_mode = true;
}

void Tape::Read6()
{
    // FIXED and SILI must not both be set
    if ((GetCdbByte(1) & 0x03) == 0x03) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int count = GetByteCount();
    if (count) {
        if (static_cast<off_t>(position + count) > GetFileSize()) {
            // End-of-partition
            SetEom();
            SetInformation(count);
            throw scsi_exception(sense_key::medium_error);
        }

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
        if (static_cast<off_t>(position + byte_count) > GetFileSize()) {
            // End-of-partition
            SetEom();
            SetInformation(byte_count);
            throw scsi_exception(sense_key::medium_error);
        }

        GetController()->SetTransferSize(byte_count, byte_count % GetBlockSize() ? byte_count : GetBlockSize());

        DataOutPhase(byte_count % GetBlockSize() ? byte_count : GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

int Tape::GetVariableBlockSize()
{
    const int length = FindNextObject(object_type::BLOCK, 0);

    // Check for incorrect block length
    if (length != GetController()->GetChunkSize()) {
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

    // Position before the actual data block
    position += sizeof(meta_data_t);

    return length;
}

int Tape::ReadData(span<uint8_t> buf)
{
    CheckReady();

    const int length = tar_mode ? GetBlockSize() : GetVariableBlockSize();

    file.seekg(position, ios::beg);
    file.read((char*)buf.data(), length);
    if (file.fail()) {
        file.clear();
        ++read_error_count;
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }

    position += length;
    ++block_location;
    ++blocks_read;

    return length;
}

int Tape::WriteData(span<const uint8_t> buf, scsi_command)
{
    CheckReady();

    const int length = GetController()->GetChunkSize();

    if (static_cast<off_t>(position + length) > GetFileSize()) {
        throw scsi_exception(sense_key::volume_overflow);
    }

    if (!tar_mode) {
        WriteMetaData(object_type::BLOCK, length);
    }

    file.seekp(position, ios::beg);
    file.write((const char*)buf.data(), length);
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    byte_count -= length;
    position += length;
    ++block_location;

    if (!tar_mode) {
        WriteMetaData(object_type::END_OF_DATA);
    }

    file.flush();
    if (file.fail()) {
        file.clear();
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    return length;
}

void Tape::Open()
{
    assert(!IsReady());

    // Block size and number of blocks
    if (!SetBlockSize(GetConfiguredBlockSize() ? GetConfiguredBlockSize() : 512)) {
        throw io_exception("Invalid block size");
    }

    SetBlockCount(static_cast<uint32_t>(GetFileSize() / GetBlockSize()));

    ValidateFile();

    position = 0;
    block_location = 0;
    filesize = GetFileSize();
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
    CheckWritePreconditions();

    // Check Long bit. Like with an HP35470A a long erase erases everything, otherwise only EOD is written.
    if (GetCdbByte(1) & 0x01) {
        Erase();

        // After a Long erase according to the standard the position is undefined, for SCSI2Pi it is 0
        position = 0;
    }

    WriteMetaData(object_type::END_OF_DATA);

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
    position = 0;
    block_location = 0;

    StatusPhase();
}

void Tape::Space6()
{
    const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2);
    if (count < 0) {
        LogError("Reverse spacing is not supported");
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int code = GetCdbByte(1) & 0x07;

    if (tar_mode) {
        SpaceTarMode(code, count);
    }
    else {
        SpaceTapMode(code, count);
    }

    StatusPhase();
}

void Tape::SpaceTarMode(int code, int32_t count)
{
    switch (code) {
    case object_type::BLOCK:
        if (static_cast<int64_t>(block_location) + count >= 0) {
            position += count * GetBlockSize();
            block_location += count;
        }
        else {
            position = 0;
            block_location = 0;
        }
        break;

    case object_type::END_OF_DATA:
        position = GetFileSize();
        block_location = position / GetBlockSize();
        break;

    case object_type::FILEMARK:
        LogError("Spacing over filemarks in tar-file compatibility mode is not possible");
        [[fallthrough]];

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }
}

void Tape::SpaceTapMode(int code, int32_t count)
{
    switch (code) {
    case object_type::BLOCK:
    case object_type::FILEMARK:
        if (count) {
            FindNextObject(static_cast<object_type>(code), count);
        }
        break;

    case object_type::END_OF_DATA:
        FindNextObject(object_type::END_OF_DATA, 0);
        break;

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }
}

void Tape::WriteFilemarks6()
{
    // Setmarks are not supported
    if (GetCdbByte(1) & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckWritePreconditions();

    if (!tar_mode) {
        const int count = GetCdbInt24(2);
        for (int i = 0; i < count; i++) {
            WriteMetaData(object_type::FILEMARK);
        }

        if (count) {
            WriteMetaData(object_type::END_OF_DATA);
        }
    }
    else {
        LogWarn("Writing filemarks in tar-file compatibility mode is not possible, command is ignored");
    }

    StatusPhase();
}

// This is a potentially long-running operation because filemarks have to be skipped
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
        position = 0;
        block_location = 0;

        // BT
        if (GetCdbByte(1) & 0x01) {
            position = identifier;
            block_location = position / GetBlockSize();
        } else {
            FindNextObject(object_type::BLOCK, identifier);
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
    if (static_cast<off_t>(position) >= filesize) {
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
    CheckWritePreconditions();

    if (position) {
        throw scsi_exception(sense_key::illegal_request, asc::sequential_positioning_error);
    }

    Erase();

    position = 0;
    block_location = 0;

    WriteMetaData(object_type::END_OF_DATA);

    StatusPhase();
}

void Tape::WriteMetaData(Tape::object_type type, uint32_t size)
{
    assert(size < 65536);
    assert(!size || type == object_type::BLOCK);

    if (static_cast<off_t>(position + sizeof(meta_data_t)) > GetFileSize()) {
        throw scsi_exception(sense_key::volume_overflow);
    }

    meta_data_t meta_data;
    memcpy(meta_data.magic.data(), MAGIC, 4);
    meta_data.type = type;
    meta_data.reserved = 0;
    // TODO Write previous position
    SetInt64(meta_data.prev_position, 0, -1);
    SetInt64(meta_data.next_position, 0, position + sizeof(meta_data_t) + size);

    file.seekp(position, ios::beg);
    file.write((const char*)&meta_data, sizeof(meta_data_t));
    file.flush();
    if (file.fail()) {
        file.clear();
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    // The current position is always before end-of-data
    if (type != object_type::END_OF_DATA) {
        position += sizeof(meta_data_t);
    }
}

uint32_t Tape::FindNextObject(Tape::object_type type, int64_t count)
{
    assert(count >= 0);

    while (true) {
        file.seekg(position, ios::beg);

        meta_data_t meta_data;
        file.read((char*)&meta_data, sizeof(meta_data_t));
        if (file.fail()) {
            file.clear();
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        // The magic is a safeguard against random data that look like objects
        if (memcmp(meta_data.magic.data(), MAGIC, 4)) {
            // This is the next possible object position
            position += 2;
            continue;
        }

        const auto size = static_cast<uint32_t>(GetInt64(meta_data.next_position, 0) - position - sizeof(meta_data_t));

        if (meta_data.type == type) {
            --count;

            if (count < 0) {
                LogTrace(fmt::format("Next object location is {0}, object type is {1}", position,
                    static_cast<uint8_t>(type)));

                return size;
            }
        }

        // End-of-partition side
        position = GetInt64(meta_data.next_position, 0);

        // End-of-partition
        if (static_cast<off_t>(position) >= filesize) {
            LogTrace("Encountered end-of-partition");
            SetInformation(count);
            SetEom();
            throw scsi_exception(sense_key::medium_error);
        }

        // End-of-data while spacing over blocks or filemarks
        if (meta_data.type == object_type::END_OF_DATA && (type != object_type::END_OF_DATA)) {
            LogTrace(fmt::format("Encountered end-of-data while spacing over {}",
                    type == object_type::BLOCK ? "blocks" : "filemarks"));
            SetInformation(count);
            throw scsi_exception(sense_key::blank_check);
        }

        // Terminate while spacing over blocks and a filemark is found
        if (meta_data.type == object_type::FILEMARK && type == object_type::BLOCK) {
            LogTrace("Encountered filemark while spacing over blocks");
            SetInformation(count);
            SetFilemark();
            throw scsi_exception(sense_key::no_sense);
        }

        if (meta_data.type == object_type::BLOCK) {
            ++block_location;
        }
    }
}

uint32_t Tape::GetByteCount() const
{
    const bool fixed = GetCdbByte(1) & 0x01;
    const int32_t count = fixed ?
            GetSignedInt24(GetController()->GetCdb(), 2) * GetBlockSize() :
            GetSignedInt24(GetController()->GetCdb(), 2);

    // The non-fixed block size must be a multiple of 4 (see SSC-5)
    if (!fixed && count % 4) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    LogTrace(fmt::format("Position: {0}, byte count: {1}", position, count));

    return count;
}

void Tape::Erase()
{
    file.seekp(position, ios::beg);

    // Erase in 4096 byte chunks
    vector<byte> buf(4096);

    uint64_t remaining = filesize - position;
    while (remaining > 0) {
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
