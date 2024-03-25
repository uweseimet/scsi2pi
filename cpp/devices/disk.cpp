//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
//   Copyright (C) 2010-2015 isaki@NetBSD.org
//   Copyright (C) 2010 Y.Sugahara
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "linux_cache.h"
#include "disk_cache.h"
#include "disk.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

bool Disk::Init(const param_map &params)
{
    StorageDevice::Init(params);

    // REZERO implementation is identical with Seek
    AddCommand(scsi_command::cmd_rezero, [this]
        {
            ReAssignBlocks();
        });
    AddCommand(scsi_command::cmd_format_unit, [this]
        {
            FormatUnit();
        });
    AddCommand(scsi_command::cmd_reassign_blocks, [this]
        {
            ReAssignBlocks();
        });
    AddCommand(scsi_command::cmd_read6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::cmd_write6, [this]
        {
            Write6();
        });
    AddCommand(scsi_command::cmd_seek6, [this]
        {
            Seek6();
        });
    AddCommand(scsi_command::cmd_start_stop, [this]
        {
            StartStopUnit();
        });
    AddCommand(scsi_command::cmd_prevent_allow_medium_removal, [this]
        {
            PreventAllowMediumRemoval();
        });
    AddCommand(scsi_command::cmd_read_capacity10, [this]
        {
            ReadCapacity10();
        });
    AddCommand(scsi_command::cmd_read10, [this]
        {
            Read10();
        });
    AddCommand(scsi_command::cmd_write10, [this]
        {
            Write10();
        });
    AddCommand(scsi_command::cmd_read_long10, [this]
        {
            ReadLong10();
        });
    AddCommand(scsi_command::cmd_write_long10, [this]
        {
            WriteLong10();
        });
    AddCommand(scsi_command::cmd_write_long16, [this]
        {
            WriteLong16();
        });
    AddCommand(scsi_command::cmd_seek10, [this]
        {
            Seek10();
        });
    AddCommand(scsi_command::cmd_verify10, [this]
        {
            Verify(RW10);
        });
    AddCommand(scsi_command::cmd_synchronize_cache10, [this]
        {
            SynchronizeCache();
        });
    AddCommand(scsi_command::cmd_synchronize_cache16, [this]
        {
            SynchronizeCache();
        });
    AddCommand(scsi_command::cmd_read_defect_data10, [this]
        {
            ReadDefectData10();
        });
    AddCommand(scsi_command::cmd_read16, [this]
        {
            Read16();
        });
    AddCommand(scsi_command::cmd_write16, [this]
        {
            Write16();
        });
    AddCommand(scsi_command::cmd_verify16, [this]
        {
            Verify(RW16);
        });
    AddCommand(scsi_command::cmd_read_capacity16_read_long16, [this]
        {
            ReadCapacity16_ReadLong16();
        });

    return true;
}

void Disk::CleanUp()
{
    FlushCache();

    StorageDevice::CleanUp();
}

void Disk::Dispatch(scsi_command cmd)
{
    // Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
    if (cmd != scsi_command::cmd_inquiry && cmd != scsi_command::cmd_request_sense && IsMediumChanged()) {
        assert(IsRemovable());

        SetMediumChanged(false);

        throw scsi_exception(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }

    StorageDevice::Dispatch(cmd);
}

void Disk::ValidateFile()
{
    StorageDevice::ValidateFile();

    if (!SetUpCache()) {
        throw io_exception("Can't initialize cache");
    }
}

bool Disk::SetUpCache()
{
    assert(caching_mode != PbCachingMode::DEFAULT);

    if (!supported_sector_sizes.contains(sector_size)) {
        warn("Using non-standard sector size of {} bytes", sector_size);
        if (caching_mode == PbCachingMode::PISCSI) {
            caching_mode = PbCachingMode::LINUX;
            // LogInfo() does not work here because at initialization time the device ID is not yet set
            info("Switched caching mode to '{}'", PbCachingMode_Name(caching_mode));
        }
    }

    return InitCache(GetFilename());
}

bool Disk::InitCache(const string &path)
{
    if (caching_mode == PbCachingMode::PISCSI) {
        cache = make_shared<DiskCache>(path, sector_size, static_cast<uint32_t>(GetBlockCount()));
    }
    else {
        cache = make_shared<LinuxCache>(path, sector_size, static_cast<uint32_t>(GetBlockCount()),
            caching_mode == PbCachingMode::WRITE_THROUGH);
    }

    return cache->Init();
}

void Disk::FlushCache()
{
    if (cache && IsReady()) {
        cache->Flush();
    }
}

void Disk::FormatUnit()
{
    CheckReady();

    // FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
    if ((GetController()->GetCdbByte(1) & 0x10) && GetController()->GetCdbByte(4)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    StatusPhase();
}

void Disk::Read(access_mode mode)
{
    const auto& [valid, start, count] = CheckAndGetStartAndCount(mode);
    if (valid) {
        next_sector = start;

        sector_transfer_count = caching_mode == PbCachingMode::LINUX_OPTIMIZED ? count : 1;

        GetController()->SetTransferSize(count * GetSectorSizeInBytes(), sector_transfer_count * GetSectorSizeInBytes());

        GetController()->SetCurrentLength(count * GetSectorSizeInBytes());
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
    else {
        StatusPhase();
    }
}

void Disk::Write(access_mode mode)
{
    if (IsProtected()) {
        throw scsi_exception(sense_key::data_protect, asc::write_protected);
    }

    const auto& [valid, start, count] = CheckAndGetStartAndCount(mode);
    WriteVerify(start, count, valid);
}

void Disk::Verify(access_mode mode)
{
    // A transfer length of 0 is legal
    const auto& [_, start, count] = CheckAndGetStartAndCount(mode);

    // Flush the cache according to the specification
    FlushCache();

    WriteVerify(start, count, false);
}

void Disk::WriteVerify(uint64_t start, uint32_t count, bool data_out)
{
    if (data_out) {
        next_sector = start;

        sector_transfer_count = caching_mode == PbCachingMode::LINUX_OPTIMIZED ? count : 1;

        GetController()->SetTransferSize(count * GetSectorSizeInBytes(), sector_transfer_count * GetSectorSizeInBytes());

        DataOutPhase(sector_transfer_count * GetSectorSizeInBytes());
    }
    else {
        StatusPhase();
    }
}

void Disk::ReadLong10()
{
    ReadWriteLong(ValidateBlockAddress(RW10), GetInt16(GetController()->GetCdb(), 7), false);
}

void Disk::WriteLong10()
{
    ReadWriteLong(ValidateBlockAddress(RW10), GetInt16(GetController()->GetCdb(), 7), true);
}

void Disk::ReadLong16()
{
    ReadWriteLong(ValidateBlockAddress(RW16), GetInt16(GetController()->GetCdb(), 12), false);
}

void Disk::WriteLong16()
{
    ReadWriteLong(ValidateBlockAddress(RW16), GetInt16(GetController()->GetCdb(), 12), true);
}

void Disk::ReadWriteLong(uint64_t sector, uint32_t length, bool write)
{
    if (!length) {
        StatusPhase();
        return;
    }

    auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
    if (!linux_cache) {
        // FUll READ/WRITE LONG support requires an appropriate caching mode
        FlushCache();
        caching_mode = PbCachingMode::LINUX;
        InitCache(GetFilename());
        linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
        LogInfo(fmt::format("Switched caching mode to '{}'", PbCachingMode_Name(caching_mode)));
    }

    if (length > sector_size) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckReady();

    GetController()->SetTransferSize(length, length);

    if (write) {
        next_sector = sector;

        DataOutPhase(length);
    }
    else {
        ++sector_read_count;

        GetController()->SetCurrentLength(length);
        DataInPhase(linux_cache->ReadLong(GetController()->GetBuffer(), sector, length));
    }
}


void Disk::StartStopUnit()
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
        SetFilename(last_filename);
        if (!ReserveFile()) {
            last_filename.clear();
            throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
        }

        SetMediumChanged(true);
    }

    StatusPhase();
}

void Disk::PreventAllowMediumRemoval()
{
    CheckReady();

    const bool lock = GetController()->GetCdbByte(4) & 0x01;

    LogTrace(lock ? "Locking medium" : "Unlocking medium");

    SetLocked(lock);

    StatusPhase();
}

void Disk::SynchronizeCache()
{
    FlushCache();

    StatusPhase();
}

void Disk::ReadDefectData10() const
{
    const size_t allocation_length = min(static_cast<size_t>(GetInt16(GetController()->GetCdb(), 7)),
        static_cast<size_t>(4));

    GetController()->SetCurrentLength(static_cast<int>(allocation_length));

    // The defect list is empty
    fill_n(GetController()->GetBuffer().begin(), allocation_length, 0);

    DataInPhase(static_cast<int>(allocation_length));
}

bool Disk::Eject(bool force)
{
    const bool status = PrimaryDevice::Eject(force);
    if (status) {
        FlushCache();
        cache.reset();

        last_filename = GetFilename();

        // The image file for this drive is not in use anymore
        UnreserveFile();

        sector_read_count = 0;
        sector_write_count = 0;
    }

    return status;
}

int Disk::ModeSense6(cdb_t cdb, vector<uint8_t> &buf) const
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
        SetInt32(buf, 4, static_cast<uint32_t>(GetBlockCount()));
        SetInt32(buf, 8, GetSectorSizeInBytes());

        size = 12;
    }

    size = AddModePages(cdb, buf, size, length, 255);

    // The size field does not count itself
    buf[0] = (uint8_t)(size - 1);

    return size;
}

int Disk::ModeSense10(cdb_t cdb, vector<uint8_t> &buf) const
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
        const uint64_t disk_blocks = GetBlockCount();
        const uint32_t disk_size = GetSectorSizeInBytes();

        // Check LLBAA for short or long block descriptor
        if (!(cdb[1] & 0x10) || disk_blocks <= 0xffffffff) {
            // Mode parameter header, block descriptor length
            buf[7] = 0x08;

            // Short LBA mode parameter block descriptor (number of blocks and block length)
            SetInt32(buf, 8, static_cast<uint32_t>(disk_blocks));
            SetInt32(buf, 12, disk_size);

            size = 16;
        }
        else {
            // Mode parameter header, LONGLBA
            buf[4] = 0x01;

            // Mode parameter header, block descriptor length
            buf[7] = 0x10;

            // Long LBA mode parameter block descriptor (number of blocks and block length)
            SetInt64(buf, 8, disk_blocks);
            SetInt32(buf, 20, disk_size);

            size = 24;
        }
    }

    size = AddModePages(cdb, buf, size, length, 65535);

    // The size fields do not count themselves
    SetInt16(buf, 0, size - 2);

    return size;
}

void Disk::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    // Page 1 (read-write error recovery)
    if (page == 0x01 || page == 0x3f) {
        AddReadWriteErrorRecoveryPage(pages, changeable);
    }

    // Page 2 (disconnect-reconnect)
    if (page == 0x02 || page == 0x3f) {
        AddDisconnectReconnectPage(pages, changeable);
    }

    // Page 7 (verify error recovery)
    if (page == 0x07 || page == 0x3f) {
        AddVerifyErrorRecoveryPage(pages, changeable);
    }

    // Page 8 (caching)
    if (page == 0x08 || page == 0x3f) {
        AddCachingPage(pages, changeable);
    }

    // Page 10 (control mode)
    if (page == 0x0a || page == 0x3f) {
        AddControlModePage(pages, changeable);
    }

    // Page code 48
    if (page == 0x30 || page == 0x3f) {
        AddAppleVendorPage(pages, changeable);
    }

    // Page (vendor-specific)
    AddVendorPages(pages, page, changeable);
}

void Disk::AddReadWriteErrorRecoveryPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(12);

    // TB, PER, DTE (required for OpenVMS/VAX compatibility, see PiSCSI issue #1117)
    buf[2] = (byte)0x26;

    // Read/write retry count and recovery time limit are those of an IBM DORS-39130 drive
    buf[3] = (byte)1;
    buf[8] = (byte)1;
    buf[11] = (byte)218;

    pages[1] = buf;
}

void Disk::AddDisconnectReconnectPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(16);

    // For an IBM DORS-39130 drive all fields are 0

    pages[2] = buf;
}

void Disk::AddVerifyErrorRecoveryPage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(12);

    // The page data are those of an IBM DORS-39130 drive

    // Verify retry count
    buf[3] = (byte)1;

    pages[7] = buf;
}

void Disk::AddCachingPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(12);

    // No changeable area
    if (changeable) {
        pages[8] = buf;

        return;
    }

    // Only read cache is valid

    // Disable pre-fetch transfer length
    SetInt16(buf, 0x04, -1);

    // Maximum pre-fetch
    SetInt16(buf, 0x08, -1);

    // Maximum pre-fetch ceiling
    SetInt16(buf, 0x0a, -1);

    pages[8] = buf;
}

void Disk::AddControlModePage(map<int, vector<byte>> &pages, bool) const
{
    vector<byte> buf(8);

    // For an IBM DORS-39130 drive all fields are 0

    pages[10] = buf;
}

void Disk::AddAppleVendorPage(map<int, vector<byte>> &pages, bool changeable) const
{
    // Needed for SCCD for stock Apple driver support and stock Apple HD SC Setup
    pages[48] = vector<byte>(24);

    // No changeable area
    if (!changeable) {
        constexpr const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
        memcpy(&pages[48][2], APPLE_DATA, sizeof(APPLE_DATA) - 1);
    }
}

void Disk::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length)
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    // PF
    if (!(cdb[1] & 0x10)) {
        // Vendor-specific parameters (SCSI-1) are not supported.
        // Do not report an error in order to support Apple's HD SC Setup.
        return;
    }

    // The page data are optional
    if (!length) {
        return;
    }

    int size = GetSectorSizeInBytes();

    int offset = EvaluateBlockDescriptors(cmd, span(buf.data(), length), size);
    length -= offset;

    map<int, vector<byte>> pages;
    SetUpModePages(pages, 0x3f, true);

    // Parse the pages
    while (length > 0) {
        const int page_code = buf[offset];

        const auto &it = pages.find(page_code);
        if (it == pages.end()) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        if (length < 2) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        // The page size field does not count itself and the page code field
        const size_t page_size = buf[offset + 1] + 2;

        // The page size in the parameters must match the actual page size
        if (it->second.size() != page_size || page_size > static_cast<size_t>(length)) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        switch (page_code) {
        // Read-write/Verify error recovery pages
        case 0x01:
        case 0x07:
            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            break;

        // Format device page
        case 0x03:
            // With this page the sector size for a subsequent FORMAT can be selected, but only a few drives
            // support this, e.g. FUJITSU M2624S.
            // We are fine as long as the permanent current sector size remains unchanged.
            VerifySectorSizeChange(GetInt16(buf, offset + 12), false);
            break;

        default:
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        length -= page_size;
        offset += page_size;
    }

    ChangeSectorSize(size);
}

int Disk::EvaluateBlockDescriptors(scsi_command cmd, span<const uint8_t> buf, int &size) const
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    const size_t required_length = cmd == scsi_command::cmd_mode_select10 ? 8 : 4;
    if (buf.size() < required_length) {
        throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
    }

    const size_t descriptor_length = cmd == scsi_command::cmd_mode_select10 ? GetInt16(buf, 6) : buf[3];
    if (buf.size() < descriptor_length + required_length) {
        throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
    }

    // Check for temporary sector size change in first block descriptor
    if (descriptor_length && buf.size() >= required_length + 8) {
        size = VerifySectorSizeChange(GetInt16(buf, static_cast<int>(required_length) + 6), true);
    }

    return static_cast<int>(descriptor_length + required_length);
}

int Disk::VerifySectorSizeChange(int requested_size, bool temporary) const
{
    if (requested_size == static_cast<int>(GetSectorSizeInBytes())) {
        return requested_size;
    }

    // Simple consistency check
    if (requested_size && !(requested_size % 4)) {
        if (temporary) {
            return requested_size;
        }
        else {
            LogWarn(fmt::format(
                "Sector size change from {0} to {1} bytes requested. Configure the sector size in the s2p settings.",
                GetSectorSizeInBytes(), requested_size));
        }
    }

    throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
}

int Disk::ReadData(span<uint8_t> buf)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    if (!cache->ReadSectors(buf, static_cast<uint32_t>(next_sector), sector_transfer_count)) {
        throw scsi_exception(sense_key::medium_error, asc::read_fault);
    }

    next_sector += sector_transfer_count;
    sector_read_count += sector_transfer_count;

    return GetSectorSizeInBytes() * sector_transfer_count;
}

int Disk::WriteData(span<const uint8_t> buf, scsi_command command)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    if (command == scsi_command::cmd_write_long10 || command == scsi_command::cmd_write_long16) {
        auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
        assert(linux_cache);

        const auto length = linux_cache->WriteLong(buf, next_sector, GetController()->GetChunkSize());
        if (!length) {
            throw scsi_exception(sense_key::medium_error, asc::write_fault);
        }

        ++sector_write_count;

        return length;
    }

    if ((command != scsi_command::cmd_verify10 && command != scsi_command::cmd_verify16)
        && !cache->WriteSectors(buf, static_cast<uint32_t>(next_sector), sector_transfer_count)) {
        throw scsi_exception(sense_key::medium_error, asc::write_fault);
    }

    next_sector += sector_transfer_count;
    sector_write_count += sector_transfer_count;

    return GetSectorSizeInBytes() * sector_transfer_count;
}

void Disk::ReAssignBlocks()
{
    CheckReady();

    StatusPhase();
}

void Disk::Seek6()
{
    const auto& [valid, _, __] = CheckAndGetStartAndCount(SEEK6);
    if (valid) {
        CheckReady();
    }

    StatusPhase();
}

void Disk::Seek10()
{
    const auto& [valid, _, __] = CheckAndGetStartAndCount(SEEK10);
    if (valid) {
        CheckReady();
    }

    StatusPhase();
}

void Disk::ReadCapacity10()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
    }

    vector<uint8_t> &buf = GetController()->GetBuffer();

    // If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
    const uint64_t capacity = GetBlockCount() - 1;
    SetInt32(buf, 0, static_cast<uint32_t>(capacity > 0xffffffff ? -1 : capacity));
    SetInt32(buf, 4, sector_size);

    DataInPhase(8);
}

void Disk::ReadCapacity16()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
    }

    vector<uint8_t> &buf = GetController()->GetBuffer();
    fill_n(buf.begin(), 32, 0);

    SetInt64(buf, 0, GetBlockCount() - 1);
    SetInt32(buf, 8, sector_size);

    DataInPhase(min(32, static_cast<int>(GetInt32(GetController()->GetCdb(), 10))));
}

void Disk::ReadCapacity16_ReadLong16()
{
    // The service action determines the actual command
    switch (GetController()->GetCdbByte(1) & 0x1f) {
    case 0x10:
        ReadCapacity16();
        break;

    case 0x11:
        ReadLong16();
        break;

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
        break;
    }
}

uint64_t Disk::ValidateBlockAddress(access_mode mode) const
{
    // The RelAdr bit is only permitted with linked commands
    if (mode == RW10 && GetController()->GetCdbByte(1) & 0x01) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint64_t sector =
        mode == RW16 ? GetInt64(GetController()->GetCdb(), 2) : GetInt32(GetController()->GetCdb(), 2);

    if (sector > GetBlockCount()) {
        LogTrace(
            fmt::format("Capacity of {0} sector(s) exceeded: Trying to access sector {1}", GetBlockCount(), sector));
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    }

    return sector;
}

tuple<bool, uint64_t, uint32_t> Disk::CheckAndGetStartAndCount(access_mode mode) const
{
    uint64_t start;
    uint32_t count;

    if (mode == RW6 || mode == SEEK6) {
        start = GetInt24(GetController()->GetCdb(), 1);

        count = GetController()->GetCdbByte(4);
        if (!count) {
            count = 256;
        }
    }
    else {
        start = mode == RW16 ? GetInt64(GetController()->GetCdb(), 2) : GetInt32(GetController()->GetCdb(), 2);

        if (mode == RW16) {
            count = GetInt32(GetController()->GetCdb(), 10);
        }
        else if (mode != SEEK10) {
            count = GetInt16(GetController()->GetCdb(), 7);
        }
        else {
            count = 0;
        }
    }

    LogTrace(fmt::format("READ/WRITE/VERIFY/SEEK, start sector: {0}, sector count: {1}", start, count));

    // Check capacity
    if (const uint64_t capacity = GetBlockCount(); !capacity || start + count > capacity) {
        LogTrace(
            fmt::format("Capacity of {0} sector(s) exceeded: Trying to access sector {1}, sector count {2}", capacity,
                start, count));
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    }

    // Do not process 0 blocks
    return tuple(count || mode == SEEK6 || mode == SEEK10, start, count);
}

void Disk::ChangeSectorSize(uint32_t new_size)
{
    if (!supported_sector_sizes.contains(new_size) && new_size % 4) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
    }

    const auto current_size = GetSectorSizeInBytes();
    if (new_size != current_size) {
        const uint64_t capacity = current_size * GetBlockCount();
        SetSectorSizeInBytes(new_size);
        SetBlockCount(static_cast<uint32_t>(capacity / new_size));

        FlushCache();
        if (cache) {
            SetUpCache();
        }

        LogTrace(fmt::format("Changed sector size from {0} to {1} bytes", current_size, new_size));
    }
}

bool Disk::SetSectorSizeInBytes(uint32_t size)
{
    if (!supported_sector_sizes.contains(size) && configured_sector_size != size) {
        return false;
    }

    sector_size = size;

    return true;
}

bool Disk::SetConfiguredSectorSize(uint32_t configured_size)
{
    if (!configured_size || configured_size % 4
        || (!supported_sector_sizes.contains(configured_size) && GetType() != PbDeviceType::SCHD)) {
        return false;
    }

    configured_sector_size = configured_size;

    return true;
}

vector<PbStatistics> Disk::GetStatistics() const
{
    vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

    // Enrich cache statistics with device information before adding them to device statistics
    if (cache) {
        for (auto &s : cache->GetStatistics(IsReadOnly())) {
            s.set_id(GetId());
            s.set_unit(GetLun());
            statistics.push_back(s);
        }
    }

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(SECTOR_READ_COUNT);
    s.set_value(sector_read_count);
    statistics.push_back(s);

    if (!IsReadOnly()) {
        s.set_key(SECTOR_WRITE_COUNT);
        s.set_value(sector_write_count);
        statistics.push_back(s);
    }

    return statistics;
}
