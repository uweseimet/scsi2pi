//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// This implementation treats files with a .tar extension in a special way, so that regular .tar files can be used as
// image files. Filemark, end-of-data and spacing support is not possible for these files, because .tar files do not
// contain any meta data. Therefore, SCSI2Pi only supports filemarks and end-of-data for image files that do *not*
// have the extension .tar, e.g. files with the .tap extension.
// Note that tar (actually the Linux tape device driver) tries to create a filemark as an end-of-data marker when
// writing to a tape device, but not when writing to a local .tar file.
// Reverse spacing (optional anyway) is not supported because the object type of the previous objects cannot easily be
// determined. One would need a special data encoding to move backwards and find the beginning of a block or filemark.
// Splitting each data byte and writing it as two low nibbles, while reserving the high nibble of each byte for meta
// data, might be such a format. This would double the image file size, but this should not be an issue nowadays.
// Implementing this is most likely not worth the effort, though.
// Gap handling is device-defined and does nothing, which is SCSI-compliant.
// tap image files must be erased before first use, to ensure that they start with an end-of-data marker. I order to
// detect
// Note that the format of non-tar files may change in future SCSI2Pi releases, e.g. in order to add reverse spacing.
//
//---------------------------------------------------------------------------

#include "tape.h"
#include "base/memory_util.h"
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
    SetLockable(true);
}

bool Tape::SetUp()
{
    AddCommand(scsi_command::cmd_read6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::cmd_write6, [this]
        {
            Write6();
        });
    AddCommand(scsi_command::cmd_erase6, [this]
        {
            Erase6();
        });
    AddCommand(scsi_command::cmd_read_block_limits, [this]
        {
            ReadBlockLimits();
        });
    AddCommand(scsi_command::cmd_rewind, [this]
        {
            Rewind();
        });
    AddCommand(scsi_command::cmd_space6, [this]
        {
            Space6();
        });
    AddCommand(scsi_command::cmd_write_filemarks6, [this]
        {
            WriteFilemarks6();
        });
    AddCommand(scsi_command::cmd_locate10, [this]
        {
            Locate10();
        });
    AddCommand(scsi_command::cmd_locate16, [this]
        {
            Locate16();
        });
    AddCommand(scsi_command::cmd_read_position, [this]
        {
            ReadPosition();
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

    tar_mode = GetExtensionLowerCase(GetFilename()) == "tar";
}

void Tape::Read6()
{
    const int count = GetByteCount();
    if (count) {
        GetController()->SetTransferSize(count, GetBlockSize());

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
        GetController()->SetTransferSize(byte_count, GetBlockSize());

        DataOutPhase(GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

int Tape::ReadData(span<uint8_t> buf)
{
    CheckReady();

    if (!tar_mode) {
        if (FindNextObject(object_type::BLOCK, 0) != GetBlockSize()) {
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        position += sizeof(meta_data_t);
    }

    file.seekg(position, ios::beg);
    file.read((char*)buf.data(), GetBlockSize());
    if (file.fail()) {
        ++read_error_count;
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }

    position += GetBlockSize();
    ++block_location;

    return GetBlockSize();
}

int Tape::WriteData(span<const uint8_t> buf, scsi_command)
{
    CheckReady();

    file.seekp(position, ios::beg);

    if (!tar_mode) {
        WriteMetaData(object_type::BLOCK, GetBlockSize());
    }

    file.write((const char*)buf.data(), GetBlockSize());
    if (file.fail()) {
        ++write_error_count;
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    position += GetBlockSize();
    ++block_location;
    byte_count -= GetBlockSize();

    // Write end-of-data if this was the last block to be written
    if (!tar_mode && !byte_count) {
        WriteMetaData(object_type::END_OF_DATA, 0);
    }

    file.flush();
    if (file.fail()) {
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    return GetBlockSize();
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
    }

    return status;
}

vector<uint8_t> Tape::InquiryInternal() const
{
    return HandleInquiry(device_type::sequential_access, true);
}

void Tape::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    StorageDevice::SetUpModePages(pages, page, changeable);

    // Page 0 (mode block descriptor), used by tools like tar.
    // Due to its format page 0 cannot be returned with a page list. This has been verified with an HP 35470A.
    if (page == 0x00) {
        AddModeBlockDescriptor(pages);
    }

    // Page 16 (device configuration)
    if (page == 0x10 || page == 0x3f) {
        AddDeviceConfigurationPage(pages, changeable);
    }

    // Page 17 (medium partition page)
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
    SetInt24(buf, 9, GetBlockSize());

    pages[0] = buf;
}

void Tape::AddDeviceConfigurationPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(16);

    if (!changeable) {
        // BIS (block identifiers supported)
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

    file.seekp(position, ios::beg);
    WriteMetaData(object_type::END_OF_DATA, 0);

    // Check Long bit. Like with an HP35470A a long erase erases everything, otherwise only EOD is written.
    if (GetController()->GetCdb()[1] & 0x01) {
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
                throw scsi_exception(sense_key::medium_error, asc::write_error);
            }

            remaining -= chunk;
            position += chunk;
            block_location += chunk / GetBlockSize();
        }
    }

    file.flush();
    if (file.fail()) {
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    StatusPhase();
}

void Tape::ReadBlockLimits()
{
    vector<uint8_t> &buf = GetController()->GetBuffer();
    buf[0] = 0;

    // Only Fixed mode is supported, using the configured block size
    SetInt24(buf, 1, GetBlockSize());
    SetInt16(buf, 4, GetBlockSize());

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
    if (tar_mode) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2);

    if (count < 0) {
        LogError("Reverse spacing is not supported");
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    switch (const int code = GetController()->GetCdb()[1] & 0x07; code) {
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

    StatusPhase();
}

void Tape::WriteFilemarks6()
{
    // Setmarks are not supported
    if (GetController()->GetCdb()[1] & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckWritePreconditions();

    if (!tar_mode) {
        file.seekp(position, ios::beg);

        const int count = GetInt24(GetController()->GetCdb(), 2);
        for (int i = 0; i < count; i++) {
            WriteMetaData(object_type::FILEMARK, 0);
        }

        if (count) {
            WriteMetaData(object_type::END_OF_DATA, 0);
        }
    }
    else {
        LogWarn("Writing filemarks in tar-file compatibility mode is not possible");
    }

    StatusPhase();
}

// This is a potentially long-running operation because filemarks have to be skipped
void Tape::Locate(bool locate16)
{
    // CP is not supported, BT does not make a difference
    if (GetController()->GetCdb()[1] & 0x02) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint64_t block = locate16 ? GetInt64(GetController()->GetCdb(), 4) : GetInt32(GetController()->GetCdb(), 3);

    position = 0;
    block_location = 0;

    if (tar_mode) {
        position = block * GetBlockSize();
        block_location = block;
    }
    else {
        FindNextObject(object_type::BLOCK, block);
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

    // First and last block location, BT does not make a difference
    SetInt32(buf, 4, static_cast<uint32_t>(block_location));
    SetInt32(buf, 8, static_cast<uint32_t>(block_location));

    DataInPhase(20);
}

void Tape::WriteMetaData(Tape::object_type type, uint32_t size)
{
    assert(size < 65536);

    meta_data_t meta_data;
    memcpy(meta_data.magic.data(), MAGIC, 4);
    meta_data.type = type;
    meta_data.size[0] = static_cast<uint8_t>(size >> 8);
    meta_data.size[1] = static_cast<uint8_t>(size);
    file.write((const char*)&meta_data, sizeof(meta_data));
    if (file.fail()) {
        throw scsi_exception(sense_key::medium_error, asc::write_error);
    }

    if (type != object_type::END_OF_DATA) {
        position += sizeof(meta_data);
    }
}

uint32_t Tape::FindNextObject(Tape::object_type type, int64_t count)
{
    assert(count >= 0);

    while (true) {
        file.seekg(position, ios::beg);

        meta_data_t meta_data;
        file.read((char*)&meta_data, sizeof(meta_data));
        if (file.fail()) {
            throw scsi_exception(sense_key::medium_error, asc::read_error);
        }

        // The magic is a safeguard against random data that look like objects
        if (memcmp(meta_data.magic.data(), MAGIC, 4)) {
            position += 2;
            continue;
        }

        const uint32_t size = (static_cast<uint32_t>(meta_data.size[0]) << 8) | meta_data.size[1];

        if (meta_data.type == type) {
            --count;

            if (count < 0) {
                LogTrace(fmt::format("Next object location is {0}, object type is {1}", position,
                    static_cast<uint8_t>(type)));

                return size;
            }
        }

        // End-of-partition side
        position += sizeof(meta_data) + size;

        // End-of-partition
        if (static_cast<off_t>(position) >= filesize) {
            LogTrace("Encountered end-of-partition while spacing");
            SetInformation(count);
            SetEom();
            throw scsi_exception(sense_key::medium_error);
        }

        // End-of-data while spacing over blocks or filemarks
        if (meta_data.type == object_type::END_OF_DATA && (type != object_type::END_OF_DATA)) {
            LogTrace("Encountered end-of-data while spacing over blocks or filemarks");
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

int Tape::GetByteCount() const
{
    // Fixed and SILI must not both be set
    if ((GetController()->GetCdb()[1] & 0x03) == 0x03) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    // Only Fixed mode with the configured block size is supported
    if (!(GetController()->GetCdb()[1] & 0x01)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int32_t count = GetSignedInt24(GetController()->GetCdb(), 2) * GetBlockSize();

    if (static_cast<off_t>(position + count) > filesize) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    LogTrace(fmt::format("READ/WRITE, position: {0}, byte count: {1}", position, count));

    return count;
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
