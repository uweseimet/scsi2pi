//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "storage_device.h"
#include "base/memory_util.h"
#include <unistd.h>
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace memory_util;

StorageDevice::StorageDevice(PbDeviceType type, scsi_level level, int lun, bool supports_mode_select,
    bool supports_save_parameters, const unordered_set<uint32_t> &s)
: ModePageDevice(type, level, lun, supports_mode_select, supports_save_parameters), supported_block_sizes(s)
{
    SupportsFile(true);
    SetStoppable(true);
}

bool StorageDevice::Init(const param_map &params)
{
    ModePageDevice::Init(params);

    AddCommand(scsi_command::cmd_start_stop, [this]
        {
            StartStopUnit();
        });
    AddCommand(scsi_command::cmd_prevent_allow_medium_removal, [this]
        {
            PreventAllowMediumRemoval();
        });

    return true;
}

void StorageDevice::CleanUp()
{
    UnreserveFile();

    ModePageDevice::CleanUp();
}

void StorageDevice::Dispatch(scsi_command cmd)
{
    // Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
    if (cmd != scsi_command::cmd_inquiry && cmd != scsi_command::cmd_request_sense && IsMediumChanged()) {
        assert(IsRemovable());

        SetMediumChanged(false);

        throw scsi_exception(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }

    ModePageDevice::Dispatch(cmd);
}

void StorageDevice::StartStopUnit()
{
    const bool start = GetController()->GetCdbByte(4) & 0x01;
    const bool load = GetController()->GetCdbByte(4) & 0x02;

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
            if (IsLocked()) {
                // Cannot be ejected because it is locked
                throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
            }

            // Eject
            if (!Eject(false)) {
                throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
            }
        }
        else {
            FlushCache();
        }
    }
    else if (load && !last_filename.empty()) {
        SetFilename (last_filename);
        if (!ReserveFile()) {
            last_filename.clear();
            throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
        }

        SetMediumChanged(true);
    }

    StatusPhase();
}

void StorageDevice::PreventAllowMediumRemoval()
{
    CheckReady();

    const bool lock = GetController()->GetCdbByte(4) & 0x01;

    LogTrace(lock ? "Locking medium" : "Unlocking medium");

    SetLocked(lock);

    StatusPhase();
}

bool StorageDevice::Eject(bool force)
{
    const bool status = ModePageDevice::Eject(force);
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

bool StorageDevice::SetBlockSizeInBytes(uint32_t size)
{
    if (!supported_block_sizes.contains(size) && configured_block_size != size) {
        return false;
    }

    block_size = size;

    return true;
}

bool StorageDevice::SetConfiguredBlockSize(uint32_t configured_size)
{
    if (!configured_size || configured_size % 4
        || (!supported_block_sizes.contains(configured_size) && GetType() != SCHD)) {
        return false;
    }

    configured_block_size = configured_size;

    return true;
}

void StorageDevice::ValidateFile()
{
    if (!blocks) {
        throw io_exception("Device has 0 blocks");
    }

    if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
        throw io_exception("Image files > 2 TiB are not supported");
    }

    // TODO Check for duplicate handling of these properties (-> CommandExecutor)
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

bool StorageDevice::FileExists(string_view file)
{
    return exists(path(file));
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
        throw io_exception("Can't get size of '" + filename.string() + "': " + e.what());
    }
}

int StorageDevice::ModeSense6(cdb_t cdb, vector<uint8_t> &buf) const
{
    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
    fill_n(buf.begin(), length, 0);

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[2] = 0x80;
    }

    // Basic information
    int size = 4;

    // Add block descriptor if DBD is 0, only if ready
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Mode parameter header, block descriptor length
        buf[3] = 0x08;

        // Short LBA mode parameter block descriptor (number of blocks and block length)
        SetInt32(buf, 4, static_cast<uint32_t>(blocks));
        SetInt32(buf, 8, block_size);

        size = 12;
    }

    size = AddModePages(cdb, buf, size, length, 255);

    // The size field does not count itself
    buf[0] = (uint8_t)(size - 1);

    return size;
}

int StorageDevice::ModeSense10(cdb_t cdb, vector<uint8_t> &buf) const
{
    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
    fill_n(buf.begin(), length, 0);

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[3] = 0x80;
    }

    // Basic Information
    int size = 8;

    // Add block descriptor if DBD is 0, only if ready
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Check LLBAA for short or long block descriptor
        if (!(cdb[1] & 0x10) || blocks <= 0xffffffff) {
            // Mode parameter header, block descriptor length
            buf[7] = 0x08;

            // Short LBA mode parameter block descriptor (number of blocks and block length)
            SetInt32(buf, 8, static_cast<uint32_t>(blocks));
            SetInt32(buf, 12, block_size);

            size = 16;
        }
        else {
            // Mode parameter header, LONGLBA
            buf[4] = 0x01;

            // Mode parameter header, block descriptor length
            buf[7] = 0x10;

            // Long LBA mode parameter block descriptor (number of blocks and block length)
            SetInt64(buf, 8, blocks);
            SetInt32(buf, 20, block_size);

            size = 24;
        }
    }

    size = AddModePages(cdb, buf, size, length, 65535);

    // The size fields do not count themselves
    SetInt16(buf, 0, size - 2);

    return size;
}

vector<PbStatistics> StorageDevice::GetStatistics() const
{
    vector<PbStatistics> statistics = ModePageDevice::GetStatistics();

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(BLOCK_READ_COUNT);
    s.set_value(block_read_count);
    statistics.push_back(s);

    if (!IsReadOnly()) {
        s.set_key(BLOCK_WRITE_COUNT);
        s.set_value(block_write_count);
        statistics.push_back(s);
    }

    return statistics;
}

