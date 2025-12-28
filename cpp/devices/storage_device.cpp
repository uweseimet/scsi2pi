//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "storage_device.h"
#include <unistd.h>
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace memory_util;

string StorageDevice::SetUp()
{
    AddCommand(ScsiCommand::START_STOP, [this]
        {
            StartStopUnit();
        });
    AddCommand(ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL, [this]
        {
            PreventAllowMediumRemoval();
        });

    page_handler = make_unique<PageHandler>(*this, supports_mode_select, supports_save_parameters);

    return "";
}

void StorageDevice::CleanUp()
{
    UnreserveFile();

    PrimaryDevice::CleanUp();
}

void StorageDevice::Dispatch(ScsiCommand cmd)
{
    // Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
    if (cmd != ScsiCommand::INQUIRY && cmd != ScsiCommand::REQUEST_SENSE && IsMediumChanged()) {
        assert(IsRemovable());

        SetMediumChanged(false);

        throw ScsiException(SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);
    }

    PrimaryDevice::Dispatch(cmd);
}

void StorageDevice::CheckWritePreconditions() const
{
    if (IsProtected()) {
        throw ScsiException(SenseKey::DATA_PROTECT, Asc::WRITE_PROTECTED);
    }
}

void StorageDevice::StartStopUnit()
{
    const bool start = GetCdbByte(4) & 0x01;
    const bool load = GetCdbByte(4) & 0x02;

    if (load) {
        LogTrace(start ? "Loading medium" : "Ejecting medium");
    }
    else {
        LogTrace(start ? "Starting unit" : "Stopping unit");

        SetStopped(!start);
    }

    if (!start) {
        // Look at the eject bit and eject if necessary
        if (load) {
            if (IsLocked() || !Eject(false)) {
                throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::MEDIUM_LOAD_OR_EJECT_FAILED);
            }
        }
        else {
            FlushCache();
        }
    }
    else if (load && !last_filename.empty()) {
        SetFilename(last_filename);
        if (!ReserveFile()) {
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::MEDIUM_LOAD_OR_EJECT_FAILED);
        }

        SetMediumChanged(true);
    }

    StatusPhase();
}

void StorageDevice::PreventAllowMediumRemoval()
{
    CheckReady();

    SetLocked(GetCdbByte(4) & 0x01);

    StatusPhase();
}

bool StorageDevice::Eject(bool force)
{
    const bool status = PrimaryDevice::Eject(force);
    if (status) {
        FlushCache();

        last_filename = GetFilename();

        // The image file for this device is not in use anymore
        UnreserveFile();

        block_read_count = 0;
        block_write_count = 0;
    }

    return status;
}

void StorageDevice::ModeSelect(cdb_t cdb, data_out_t buf, int length)
{
    // The page data are optional
    if (!length) {
        return;
    }

    auto [offset, size] = EvaluateBlockDescriptors(static_cast<ScsiCommand>(cdb[0]), span(buf.data(), length),
        block_size);
    if (size) {
        block_size = size;
    }

    // PF
    if (!(cdb[1] & 0x10)) {
        // Vendor-specific parameters (all parameters in SCSI-1 are vendor-specific) are not supported.
        // Do not report an error in order to support Apple's HD SC Setup.
        return;
    }

    length -= offset;

    // Set up the available pages in order to check for the right page size below
    map<int, vector<byte>> pages;
    SetUpModePages(pages, 0x3f, true);
    const auto& [vendor, product, _] = GetProductData();
    for (const auto& [p, data] : page_handler->GetCustomModePages(vendor, product)) {
        if (data.empty()) {
            pages.erase(p);
        }
        else {
            pages[p] = data;
        }
    }

    // Parse the pages
    while (length > 0) {
        const int page_code = buf[offset];

        const auto &it = pages.find(page_code);
        if (it == pages.end()) {
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_PARAMETER_LIST);
        }

        // Page 0 can contain anything and can have any length
        if (!page_code) {
            break;
        }

        if (length < 2) {
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::PARAMETER_LIST_LENGTH_ERROR);
        }

        // The page size field does not count itself and the page code field
        const int page_size = buf[offset + 1] + 2;

        // The page size in the parameters must match the actual page size, otherwise report
        // INVALID FIELD IN PARAMETER LIST (SCSI-2 8.2.8).
        if (static_cast<int>(it->second.size()) != page_size || page_size > length) {
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_PARAMETER_LIST);
        }

        switch (page_code) {
        // Read-write/Verify error recovery and caching pages
        case 0x01:
        case 0x07:
        case 0x08:
            // Simply ignore the requested changes in the error handling or caching, they are not relevant for SCSI2Pi
            break;

            // Format device page
        case 0x03:
            // With this page the block size for a subsequent FORMAT can be selected, but only a few devices
            // support this, e.g. FUJITSU M2624S.
            // We are fine as long as the permanent current block size remains unchanged.
            VerifyBlockSizeChange(static_cast<uint32_t>(GetInt16(buf, offset + 12)), false);
            break;

        default:
            throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_PARAMETER_LIST);
        }

        length -= page_size;
        offset += page_size;
    }

    ChangeBlockSize(size);
}

pair<int, int> StorageDevice::EvaluateBlockDescriptors(ScsiCommand cmd, data_out_t buf, int size)
{
    assert(cmd == ScsiCommand::MODE_SELECT_6 || cmd == ScsiCommand::MODE_SELECT_10);

    const size_t required_length = cmd == ScsiCommand::MODE_SELECT_10 ? 8 : 4;
    if (buf.size() < required_length) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::PARAMETER_LIST_LENGTH_ERROR);
    }

    const size_t descriptor_length = cmd == ScsiCommand::MODE_SELECT_10 ? GetInt16(buf, 6) : buf[3];
    if (buf.size() < descriptor_length + required_length) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::PARAMETER_LIST_LENGTH_ERROR);
    }

    // Check for temporary block size change in first block descriptor
    if (descriptor_length && buf.size() >= required_length + 8) {
        size = VerifyBlockSizeChange(GetInt16(buf, static_cast<uint32_t>(required_length) + 6), true);
    }

    // Offset and (potentially new) size
    return {static_cast<int>(descriptor_length + required_length), size};
}

uint32_t StorageDevice::VerifyBlockSizeChange(uint32_t requested_size, bool temporary)
{
    if (requested_size == GetBlockSize()) {
        return requested_size;
    }

    // Simple consistency check
    if (requested_size && !(requested_size % 4)) {
        if (temporary) {
            return requested_size;
        }
        else {
            LogWarn(fmt::format(
                "Block size change from {0} to {1} bytes requested. Configure the block size in the s2p settings.",
                GetBlockSize(), requested_size));
        }
    }

    throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_PARAMETER_LIST);
}

void StorageDevice::ChangeBlockSize(uint32_t new_size)
{
    if (!new_size || (!GetSupportedBlockSizes().contains(new_size) && new_size % 4)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_PARAMETER_LIST);
    }

    const auto current_size = block_size;
    if (new_size != current_size) {
        block_size = new_size;
        blocks = current_size * blocks / block_size;

        LogTrace(fmt::format("Changed block size from {0} to {1} bytes", current_size, block_size));
    }
}

void StorageDevice::SetBlockSize(uint32_t size)
{
    assert(supported_block_sizes.contains(size) || configured_block_size == size);

    block_size = size;
}

bool StorageDevice::SetConfiguredBlockSize(uint32_t size)
{
    if (ValidateBlockSize(size)) {
        configured_block_size = size;

        return true;
    }

    return false;
}

bool StorageDevice::ValidateBlockSize(uint32_t size) const
{
    return supported_block_sizes.contains(size);
}

void StorageDevice::ValidateFile()
{
    GetFileSize();

    if (IsReadOnlyFile()) {
        // Permanently write-protected
        SetReadOnly(true);
        SetProtectable(false);
        SetProtected(false);
    }

    SetStopped(false);
    SetRemoved(false);
    SetLocked(false);
    SetReady(true);
}

bool StorageDevice::ReserveFile() const
{
    if (filename.empty() || reserved_files.contains(filename.string())) {
        return false;
    }

    reserved_files[filename.string()] = { GetId(), GetLun() };

    return true;
}

void StorageDevice::UnreserveFile()
{
    reserved_files.erase(filename.string());

    filename.clear();
}

id_set StorageDevice::GetIdsForReservedFile(const string &file)
{
    if (const auto &it = reserved_files.find(file); it != reserved_files.end()) {
        return {it->second.first, it->second.second};
    }

    return {-1, -1};
}

bool StorageDevice::IsReadOnlyFile() const
{
    return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
    try {
        return file_size(filename);
    }
    catch (const filesystem_error &e) {
        throw IoException("Can't get size of '" + filename.string() + "': " + e.what());
    }
}

int StorageDevice::ModeSense6(cdb_t cdb, data_in_t buf) const
{
    // Subpages are not supported
    if (cdb[3]) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const int length = min(static_cast<int>(buf.size()), cdb[4]);
    fill_n(buf.begin(), length, 0);

    int size = 0;

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[2] = 0x80;
    }

    // Basic information
    size = 4;

    // Only add block descriptor if DBD is 0
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Mode parameter header, block descriptor length
        buf[3] = 0x08;

        // Short LBA mode parameter block descriptor (number of blocks and block length)
        SetInt32(buf, 4,
            static_cast<uint32_t>(GetBlockCountForDescriptor() <= 0xffffffff ? GetBlockCountForDescriptor() : 0xffffffff));
        SetInt32(buf, 8, GetBlockSizeForDescriptor(cdb[2] & 0x40));

        size += 8;
    }

    if (cdb[2] & 0x3f) {
        size = page_handler->AddModePages(cdb, buf, size, length, 255);
    }

    // The size field does not count itself
    buf[0] = static_cast<uint8_t>(size - 1);

    return size;
}

int StorageDevice::ModeSense10(cdb_t cdb, data_in_t buf) const
{
    // Subpages are not supported
    if (cdb[3]) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const int length = min(static_cast<int>(buf.size()), GetInt16(cdb, 7));
    fill_n(buf.begin(), length, 0);

    int size = 0;

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[3] = 0x80;
    }

    // Basic information
    size = 8;

    // Only add block descriptor if DBD is 0
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Check LLBAA for short or long block descriptor
        if (!(cdb[1] & 0x10)) {
            // Mode parameter header, block descriptor length
            buf[7] = 0x08;

            // Short LBA mode parameter block descriptor (number of blocks and block length)
            SetInt32(buf, 8,
                static_cast<uint32_t>(GetBlockCountForDescriptor() <= 0xffffffff ? GetBlockCount() : 0xffffffff));
            SetInt32(buf, 12, GetBlockSizeForDescriptor(cdb[2] & 0x40));

            size += 8;
        }
        else {
            // Mode parameter header, LONGLBA
            buf[4] = 0x01;

            // Mode parameter header, block descriptor length
            buf[7] = 0x10;

            // Long LBA mode parameter block descriptor (number of blocks and block length)
            SetInt64(buf, 8, GetBlockCountForDescriptor());
            SetInt32(buf, 20, GetBlockSizeForDescriptor(cdb[2] & 0x40));

            size += 16;
        }
    }

    if (cdb[2] & 0x3f) {
        size = page_handler->AddModePages(cdb, buf, size, length, 65535);
    }

    // The size field does not count itself
    SetInt16(buf, 0, size - 2);

    return size;
}

void StorageDevice::SetUpModePages(map<int, vector<byte>> &pages, int page, bool) const
{
    // Page 1 (read-write error recovery)
    if (page == 0x01 || page == 0x3f) {
        AddReadWriteErrorRecoveryPage(pages);
    }

    // Page 2 (disconnect-reconnect)
    if (page == 0x02 || page == 0x3f) {
        AddDisconnectReconnectPage(pages);
    }

    // Page 10 (control mode)
    if (page == 0x0a || page == 0x3f) {
        AddControlModePage(pages);
    }
}

void StorageDevice::AddReadWriteErrorRecoveryPage(map<int, vector<byte>> &pages) const
{
    vector<byte> buf(12);

    // TB, PER, DTE (required for OpenVMS/VAX < 7.2 compatibility, see PiSCSI issue #1117)
    buf[2] = byte { 0x26 };

    // Read/write retry count and recovery time limit are those of an IBM DORS-39130 drive
    buf[3] = byte { 1 };
    buf[8] = byte { 1 };
    buf[11] = byte { 218 };

    pages[1] = buf;
}

void StorageDevice::AddDisconnectReconnectPage(map<int, vector<byte>> &pages) const
{
    vector<byte> buf(16);

    // For an IBM DORS-39130 drive all fields are 0

    pages[2] = buf;
}

void StorageDevice::AddControlModePage(map<int, vector<byte>> &pages) const
{
    vector<byte> buf(8);

    // For an IBM DORS-39130 drive all fields are 0

    pages[10] = buf;
}

vector<PbStatistics> StorageDevice::GetStatistics() const
{
    vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

    EnrichStatistics(statistics, CATEGORY_INFO, BLOCK_READ_COUNT, block_read_count);
    if (!IsReadOnly()) {
        EnrichStatistics(statistics, CATEGORY_INFO, BLOCK_WRITE_COUNT, block_write_count);
    }

    return statistics;
}

