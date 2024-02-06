//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

using namespace memory_util;

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
    if (IsMediumChanged()) {
        assert(IsRemovable());

        SetMediumChanged(false);

        GetController()->Error(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }
    else {
        PrimaryDevice::Dispatch(cmd);
    }
}

bool Disk::SetUpCache(bool raw)
{
    if (!supported_sector_sizes.contains(sector_size)) {
        spdlog::warn("Using non-standard sector size of {} bytes", configured_sector_size);

        if (caching_mode != PbCachingMode::LINUX && caching_mode != PbCachingMode::WRITE_THROUGH) {
            spdlog::info("Updating caching mode");
            caching_mode = PbCachingMode::LINUX;
        }
    }

    if (caching_mode == PbCachingMode::LINUX || caching_mode == PbCachingMode::WRITE_THROUGH) {
        cache = make_shared<LinuxCache>(GetFilename(), sector_size, static_cast<uint32_t>(GetBlockCount()), raw,
            caching_mode == PbCachingMode::WRITE_THROUGH);
    }
    else {
        cache = make_shared<DiskCache>(GetFilename(), sector_size, static_cast<uint32_t>(GetBlockCount()), raw);
    }

    return cache->Init();
}

bool Disk::ResizeCache(const string &path, bool raw)
{
    if (caching_mode == PbCachingMode::LINUX || caching_mode == PbCachingMode::WRITE_THROUGH) {
        cache.reset(
            new LinuxCache(path, sector_size, static_cast<uint32_t>(GetBlockCount()), raw,
                caching_mode == PbCachingMode::WRITE_THROUGH));
    }
    else {
        cache.reset(new DiskCache(path, sector_size, static_cast<uint32_t>(GetBlockCount()), raw));
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

    EnterStatusPhase();
}

void Disk::Read(access_mode mode)
{
    const auto& [valid, start, blocks] = CheckAndGetStartAndCount(mode);
    if (valid) {
        GetController()->SetTransferSize(blocks * GetSectorSizeInBytes(), GetSectorSizeInBytes());

        next_sector = start;

        GetController()->SetCurrentLength(ReadData(GetController()->GetBuffer()));

        EnterDataInPhase();
    }
    else {
        EnterStatusPhase();
    }
}

void Disk::ReadLong10()
{
    const uint64_t sector = ValidateBlockAddress(RW10);

    ReadWriteLong(sector, GetInt16(GetController()->GetCdb(), 7), false);
}

void Disk::WriteLong10()
{
    const uint64_t sector = ValidateBlockAddress(RW10);

    ReadWriteLong(sector, GetInt16(GetController()->GetCdb(), 7), true);
}

void Disk::ReadLong16()
{
    const uint64_t sector = ValidateBlockAddress(RW16);

    ReadWriteLong(sector, GetInt16(GetController()->GetCdb(), 12), false);
}

void Disk::WriteLong16()
{
    const uint64_t sector = ValidateBlockAddress(RW16);

    ReadWriteLong(sector, GetInt16(GetController()->GetCdb(), 12), true);
}

void Disk::ReadWriteLong(uint64_t sector, uint32_t length, bool write)
{
    if (!length) {
        EnterStatusPhase();
        return;
    }

    // Transfer lengths other than 0 require that caching is disabled
    auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
    if (!linux_cache) {
        spdlog::error("READ/WRITE LONG require caching to be disabled");
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    if (length > sector_size) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    CheckReady();

    GetController()->SetTransferSize(length, length);

    if (write) {
        GetController()->SetCurrentLength(length);

        next_sector = sector;

        EnterDataOutPhase();
    }
    else {
        GetController()->SetCurrentLength(linux_cache->ReadLong(GetController()->GetBuffer(), sector, length));

        ++sector_read_count;

        EnterDataInPhase();
    }
}

void Disk::Write(access_mode mode)
{
    if (IsProtected()) {
        throw scsi_exception(sense_key::data_protect, asc::write_protected);
    }

    const auto& [valid, start, blocks] = CheckAndGetStartAndCount(mode);
    WriteVerify(start, blocks, valid);
}

void Disk::Verify(access_mode mode)
{
    // Flush the cache according to the specification
    FlushCache();

    // A transfer length of 0 is legal
    const auto& [_, start, blocks] = CheckAndGetStartAndCount(mode);
    WriteVerify(start, blocks, false);
}

void Disk::WriteVerify(uint64_t start, uint32_t blocks, bool transfer)
{
    if (transfer) {
        GetController()->SetTransferSize(blocks * GetSectorSizeInBytes(), GetSectorSizeInBytes());
        GetController()->SetCurrentLength(GetSectorSizeInBytes());

        next_sector = start;

        EnterDataOutPhase();
    }
    else {
        EnterStatusPhase();
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

    EnterStatusPhase();
}

void Disk::PreventAllowMediumRemoval()
{
    CheckReady();

    const bool lock = GetController()->GetCdbByte(4) & 0x01;

    LogTrace(lock ? "Locking medium" : "Unlocking medium");

    SetLocked(lock);

    EnterStatusPhase();
}

void Disk::SynchronizeCache()
{
    FlushCache();

    EnterStatusPhase();
}

void Disk::ReadDefectData10() const
{
    const size_t allocation_length = min(static_cast<size_t>(GetInt16(GetController()->GetCdb(), 7)),
        static_cast<size_t>(4));

    // The defect list is empty
    fill_n(GetController()->GetBuffer().begin(), allocation_length, 0);
    GetController()->SetCurrentLength(static_cast<uint32_t>(allocation_length));

    EnterDataInPhase();
}

bool Disk::Eject(bool force)
{
    const bool status = PrimaryDevice::Eject(force);
    if (status) {
        FlushCache();
        cache.reset();

        // The image file for this drive is not in use anymore
        UnreserveFile();

        sector_read_count = 0;
        sector_write_count = 0;
    }

    return status;
}

int Disk::ModeSense6(cdb_t cdb, vector<uint8_t> &buf) const
{
    // Get length, clear buffer
    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
    fill_n(buf.begin(), length, 0);

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[2] = 0x80;
    }

    // Basic information
    int size = 4;

    // Add block descriptor if DBD is 0
    if (!(cdb[1] & 0x08)) {
        // Mode parameter header, block descriptor length
        buf[3] = 0x08;

        // Only if ready
        if (IsReady()) {
            // Short LBA mode parameter block descriptor (number of blocks and block length)
            SetInt32(buf, 4, static_cast<uint32_t>(GetBlockCount()));
            SetInt32(buf, 8, GetSectorSizeInBytes());
        }

        size = 12;
    }

    size = AddModePages(cdb, buf, size, length, 255);

    // The size field does not count itself
    buf[0] = (uint8_t)(size - 1);

    return size;
}

int Disk::ModeSense10(cdb_t cdb, vector<uint8_t> &buf) const
{
    // Get length, clear buffer
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
        if (!(cdb[1] & 0x10) || disk_blocks <= 0xFFFFFFFF) {
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

    int offset = EvaluateBlockDescriptors(cmd, buf, length, size);
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
        const size_t page_size = buf[offset + 1];

        // The page size in the parameters must match the actual page size
        if (it->second.size() - 2 != page_size || page_size + 2 > static_cast<size_t>(length)) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        switch (page_code) {
        // Read-write error recovery page
        case 0x01:
            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            break;

        // Format device page
        case 0x03: {
            // With this page the sector size for a subsequent FORMAT can be selected, but only a few drives
            // support this, e.g. FUJITSU M2624S.
            // We are fine as long as the permanent current sector size remains unchanged.
            VerifySectorSizeChange(GetInt16(buf, offset + 12), false);
            break;
        }

        // Verify error recovery page
        case 0x07:
            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            break;

        default:
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
            break;
        }

        // The page size field does not count itself and the page code field
        length -= page_size + 2;
        offset += page_size + 2;
    }

    ChangeSectorSize(size);
}

int Disk::EvaluateBlockDescriptors(scsi_command cmd, span<const uint8_t> buf, int length, int &size) const
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    int required_length;
    int block_descriptor_length;
    if (cmd == scsi_command::cmd_mode_select10) {
        block_descriptor_length = GetInt16(buf, 6);
        required_length = 8;
    }
    else {
        block_descriptor_length = buf[3];
        required_length = 4;
    }

    if (length < block_descriptor_length + required_length) {
        throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
    }

    // Check for temporary sector size change in first block descriptor
    if (block_descriptor_length && length >= required_length + 8) {
        size = VerifySectorSizeChange(GetInt16(buf, required_length + 6), true);
    }

    return block_descriptor_length + required_length;
}

int Disk::VerifySectorSizeChange(int requested_size, bool temporary) const
{
    if (requested_size == static_cast<int>(GetSectorSizeInBytes())) {
        return requested_size;
    }

    // Simple consistency check
    if (requested_size && !(requested_size & 0xe1ff)) {
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
    assert(next_sector < GetBlockCount());

    CheckReady();

    if (!cache->ReadSector(buf, static_cast<uint32_t>(next_sector))) {
        throw scsi_exception(sense_key::medium_error, asc::read_fault);
    }

    ++next_sector;

    ++sector_read_count;

    return GetSectorSizeInBytes();
}

int Disk::WriteData(span<const uint8_t> buf, scsi_command command)
{
    assert(next_sector < GetBlockCount());

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
        && !cache->WriteSector(buf, static_cast<uint32_t>(next_sector))) {
        throw scsi_exception(sense_key::medium_error, asc::write_fault);
    }

    ++next_sector;

    ++sector_write_count;

    return GetSectorSizeInBytes();
}

void Disk::ReAssignBlocks()
{
    CheckReady();

    EnterStatusPhase();
}

void Disk::Seek6()
{
    const auto& [valid, start, blocks] = CheckAndGetStartAndCount(SEEK6);
    if (valid) {
        CheckReady();
    }

    EnterStatusPhase();
}

void Disk::Seek10()
{
    const auto& [valid, start, blocks] = CheckAndGetStartAndCount(SEEK10);
    if (valid) {
        CheckReady();
    }

    EnterStatusPhase();
}

void Disk::ReadCapacity10()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
    }

    vector<uint8_t> &buf = GetController()->GetBuffer();

    // Create end of logical block address (blocks-1)
    uint64_t capacity = GetBlockCount() - 1;

    // If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
    if (capacity > 4294967295) {
        capacity = -1;
    }
    SetInt32(buf, 0, static_cast<uint32_t>(capacity));

    SetInt32(buf, 4, sector_size);

    GetController()->SetCurrentLength(8);

    EnterDataInPhase();
}

void Disk::ReadCapacity16()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
    }

    vector<uint8_t> &buf = GetController()->GetBuffer();

    // Create end of logical block address (blocks-1)
    SetInt64(buf, 0, GetBlockCount() - 1);

    // Create block length (1 << size)
    SetInt32(buf, 8, sector_size);

    buf[12] = 0;

    // Logical blocks per physical block: not reported (1 or more)
    buf[13] = 0;

    GetController()->SetCurrentLength(14);

    EnterDataInPhase();
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
    if (mode == RW10 && GetController()->GetCdb()[1] & 0x01) {
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
            count = 0x100;
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
    if (uint64_t capacity = GetBlockCount(); !capacity || start > capacity || start + count > capacity) {
        LogTrace(
            fmt::format("Capacity of {0} sector(s) exceeded: Trying to access sector {1}, sector count {2}", capacity,
                start, count));
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    }

    // Do not process 0 blocks
    if (!count && (mode != SEEK6 && mode != SEEK10)) {
        return tuple(false, start, count);
    }

    return tuple(true, start, count);
}

void Disk::ChangeSectorSize(uint32_t new_size)
{
    if (!supported_sector_sizes.contains(new_size)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
    }

    const auto current_size = GetSectorSizeInBytes();
    if (new_size != current_size) {
        const uint64_t capacity = current_size * GetBlockCount();
        SetSectorSizeInBytes(new_size);
        SetBlockCount(static_cast<uint32_t>(capacity / new_size));

        FlushCache();
        if (cache) {
            SetUpCache(cache->IsRawMode());
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
