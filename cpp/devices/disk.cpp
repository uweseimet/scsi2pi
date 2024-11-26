//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
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

#include "disk.h"
#include "disk_cache.h"
#include "linux_cache.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Disk::Disk(PbDeviceType type, scsi_level level, int lun, bool supports_mode_select, bool supports_save_parameters,
    const unordered_set<uint32_t> &s)
: StorageDevice(type, level, lun, supports_mode_select, supports_save_parameters, s)
{
    SetStoppable(true);
}

bool Disk::SetUp()
{
    // REZERO implementation is identical with Seek
    AddCommand(scsi_command::rezero, [this]
        {
            ReAssignBlocks();
        });
    AddCommand(scsi_command::format_unit, [this]
        {
            FormatUnit();
        });
    AddCommand(scsi_command::reassign_blocks, [this]
        {
            ReAssignBlocks();
        });
    AddCommand(scsi_command::read_6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::write_6, [this]
        {
            Write6();
        });
    AddCommand(scsi_command::seek_6, [this]
        {
            Seek6();
        });
    AddCommand(scsi_command::read_capacity_10, [this]
        {
            ReadCapacity10();
        });
    AddCommand(scsi_command::read_10, [this]
        {
            Read10();
        });
    AddCommand(scsi_command::write_10, [this]
        {
            Write10();
        });
    AddCommand(scsi_command::read_long_10, [this]
        {
            ReadLong10();
        });
    AddCommand(scsi_command::write_long_10, [this]
        {
            WriteLong10();
        });
    AddCommand(scsi_command::write_long_16, [this]
        {
            WriteLong16();
        });
    AddCommand(scsi_command::seek_10, [this]
        {
            Seek10();
        });
    AddCommand(scsi_command::verify_10, [this]
        {
            Verify(RW10);
        });
    AddCommand(scsi_command::synchronize_cache_10, [this]
        {
            SynchronizeCache();
        });
    AddCommand(scsi_command::synchronize_cache_16, [this]
        {
            SynchronizeCache();
        });
    AddCommand(scsi_command::read_defect_data_10, [this]
        {
            ReadDefectData10();
        });
    AddCommand(scsi_command::read_16, [this]
        {
            Read16();
        });
    AddCommand(scsi_command::write_16, [this]
        {
            Write16();
        });
    AddCommand(scsi_command::verify_16, [this]
        {
            Verify(RW16);
        });
    AddCommand(scsi_command::read_capacity_16_read_long_16, [this]
        {
            ReadCapacity16_ReadLong16();
        });

    return StorageDevice::SetUp();
}

void Disk::CleanUp()
{
    FlushCache();

    StorageDevice::CleanUp();
}

void Disk::ValidateFile()
{
    if (!GetBlockCount()) {
        throw io_exception("Device has 0 blocks");
    }

    StorageDevice::ValidateFile();

    if (!SetUpCache()) {
        throw io_exception("Can't initialize cache");
    }
}

bool Disk::SetUpCache()
{
    assert(caching_mode != PbCachingMode::DEFAULT);

    if (!GetSupportedBlockSizes().contains(GetBlockSize())) {
        warn("Using non-standard sector size of {} bytes", GetBlockSize());
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
        cache = make_shared<DiskCache>(path, GetBlockSize(), static_cast<uint32_t>(GetBlockCount()));
    }
    else {
        cache = make_shared<LinuxCache>(path, GetBlockSize(), static_cast<uint32_t>(GetBlockCount()),
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
    if ((GetCdbByte(1) & 0x10) && GetCdbByte(4)) {
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

        GetController()->SetTransferSize(count * GetBlockSize(), sector_transfer_count * GetBlockSize());

        GetController()->SetCurrentLength(count * GetBlockSize());
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
    else {
        StatusPhase();
    }
}

void Disk::Write(access_mode mode)
{
    CheckWritePreconditions();

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

        GetController()->SetTransferSize(count * GetBlockSize(), sector_transfer_count * GetBlockSize());

        DataOutPhase(sector_transfer_count * GetBlockSize());
    }
    else {
        StatusPhase();
    }
}

void Disk::ReadLong10()
{
    ReadWriteLong(ValidateBlockAddress(RW10), GetCdbInt16(7), false);
}

void Disk::WriteLong10()
{
    ReadWriteLong(ValidateBlockAddress(RW10), GetCdbInt16(7), true);
}

void Disk::ReadLong16()
{
    ReadWriteLong(ValidateBlockAddress(RW16), GetCdbInt16(12), false);
}

void Disk::WriteLong16()
{
    ReadWriteLong(ValidateBlockAddress(RW16), GetCdbInt16(12), true);
}

void Disk::ReadWriteLong(uint64_t sector, uint32_t length, bool write)
{
    if (write) {
        CheckWritePreconditions();
    }

    if (!length) {
        StatusPhase();
        return;
    }

    if (length > GetBlockSize()) {
        SetIli();
        SetInformation(length - GetBlockSize());
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
    if (!linux_cache) {
        // FUll READ/WRITE LONG support requires an appropriate caching mode
        FlushCache();
        caching_mode = PbCachingMode::LINUX;
        InitCache(GetFilename());
        linux_cache = static_pointer_cast<LinuxCache>(cache);
        LogInfo(fmt::format("Switched caching mode to '{}'", PbCachingMode_Name(caching_mode)));
    }

    CheckReady();

    GetController()->SetTransferSize(length, length);

    if (write) {
        next_sector = sector;

        DataOutPhase(length);
    }
    else {
        UpdateReadCount(1);

        GetController()->SetCurrentLength(length);
        DataInPhase(linux_cache->ReadLong(GetController()->GetBuffer(), sector, length));
    }
}

void Disk::SynchronizeCache()
{
    FlushCache();

    StatusPhase();
}

void Disk::ReadDefectData10() const
{
    const size_t allocation_length = min(static_cast<size_t>(GetCdbInt16(7)), static_cast<size_t>(4));

    GetController()->SetCurrentLength(static_cast<int>(allocation_length));

    // The defect list is empty
    fill_n(GetController()->GetBuffer().begin(), allocation_length, 0);

    DataInPhase(static_cast<int>(allocation_length));
}

bool Disk::Eject(bool force)
{
    const bool status = StorageDevice::Eject(force);
    if (status) {
        cache.reset();
    }

    return status;
}

void Disk::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    StorageDevice::SetUpModePages(pages, page, changeable);

    // Page 7 (verify error recovery)
    if (page == 0x07 || page == 0x3f) {
        AddVerifyErrorRecoveryPage(pages, changeable);
    }

    // Page 8 (caching)
    if (page == 0x08 || page == 0x3f) {
        AddCachingPage(pages, changeable);
    }

    // Page code 48
    if (page == 0x30 || page == 0x3f) {
        AddAppleVendorPage(pages, changeable);
    }
}

void Disk::AddVerifyErrorRecoveryPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(12);

    if (!changeable) {
        // The page data are those of an IBM DORS-39130 drive

        // Verify retry count
        buf[3] = (byte)1;
    }

    pages[7] = buf;
}

void Disk::AddCachingPage(map<int, vector<byte>> &pages, bool changeable) const
{
    vector<byte> buf(12);

    if (!changeable) {
        // Only read cache is valid

        // Disable pre-fetch transfer length
        SetInt16(buf, 0x04, -1);

        // Maximum pre-fetch
        SetInt16(buf, 0x08, -1);

        // Maximum pre-fetch ceiling
        SetInt16(buf, 0x0a, -1);
    }

    pages[8] = buf;
}

void Disk::AddAppleVendorPage(map<int, vector<byte>> &pages, bool changeable) const
{
    // Needed for SCCD for stock Apple driver support and stock Apple HD SC Setup
    pages[48] = vector<byte>(24);

    if (!changeable) {
        constexpr const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
        memcpy(&pages[48][2], APPLE_DATA, sizeof(APPLE_DATA) - 1);
    }
}

int Disk::ReadData(span<uint8_t> buf)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    if (!cache->ReadSectors(buf, static_cast<uint32_t>(next_sector), sector_transfer_count)) {
        throw scsi_exception(sense_key::medium_error, asc::read_error);
    }

    next_sector += sector_transfer_count;

    UpdateReadCount(sector_transfer_count);

    return GetBlockSize() * sector_transfer_count;
}

void Disk::WriteData(span<const uint8_t> buf, scsi_command command, int)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    if (command == scsi_command::write_long_10 || command == scsi_command::write_long_16) {
        auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
        assert(linux_cache);

        if (!linux_cache->WriteLong(buf, next_sector, GetController()->GetChunkSize())) {
            throw scsi_exception(sense_key::medium_error, asc::write_fault);
        }

        UpdateWriteCount(1);

        return;
    }

    if ((command != scsi_command::verify_10 && command != scsi_command::verify_16)
        && !cache->WriteSectors(buf, static_cast<uint32_t>(next_sector), sector_transfer_count)) {
        throw scsi_exception(sense_key::medium_error, asc::write_fault);
    }

    next_sector += sector_transfer_count;

    UpdateWriteCount(sector_transfer_count);
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
    SetInt32(buf, 4, GetBlockSize());

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
    SetInt32(buf, 8, GetBlockSize());

    DataInPhase(min(32, static_cast<int>(GetCdbInt32(10))));
}

void Disk::ReadCapacity16_ReadLong16()
{
    // The service action determines the actual command
    switch (GetCdbByte(1) & 0x1f) {
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
    // RelAdr is not supported
    if (mode == RW10 && GetCdbByte(1) & 0x01) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint64_t sector = mode == RW16 ? GetCdbInt64(2) : GetCdbInt32(2);

    if (sector > GetBlockCount()) {
        LogTrace(
            fmt::format("Capacity of {0} sector(s) exceeded: Trying to access sector {1}", GetBlockCount(), sector));
        throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
    }

    return sector;
}

void Disk::ChangeBlockSize(uint32_t new_size)
{
    const auto current_size = GetBlockSize();

    StorageDevice::ChangeBlockSize(new_size);

    if (new_size != current_size) {
        FlushCache();
        if (cache) {
            SetUpCache();
        }
    }
}

tuple<bool, uint64_t, uint32_t> Disk::CheckAndGetStartAndCount(access_mode mode) const
{
    uint64_t start;
    uint32_t count;

    if (mode == RW6 || mode == SEEK6) {
        start = GetCdbInt24(1);

        count = GetCdbByte(4);
        if (!count) {
            count = 256;
        }
    }
    else {
        start = mode == RW16 ? GetCdbInt64(2) : GetCdbInt32(2);

        if (mode == RW16) {
            count = GetCdbInt32(10);
        }
        else if (mode != SEEK10) {
            count = GetCdbInt16(7);
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

vector<PbStatistics> Disk::GetStatistics() const
{
    vector<PbStatistics> statistics = StorageDevice::GetStatistics();

    // Enrich cache statistics with device information before adding them to device statistics
    if (cache) {
        for (auto &s : cache->GetStatistics(IsReadOnly())) {
            s.set_id(GetId());
            s.set_unit(GetLun());
            statistics.push_back(s);
        }
    }

    return statistics;
}
