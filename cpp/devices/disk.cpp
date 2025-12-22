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
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "disk.h"
#include "disk_cache.h"
#include "linux_cache.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

Disk::Disk(PbDeviceType type, int lun, bool supports_mode_select, bool supports_save_parameters,
    const set<uint32_t> &s)
: StorageDevice(type, lun, supports_mode_select, supports_save_parameters, s)
{
    SetStoppable(true);
}

string Disk::SetUp()
{
    AddCommand(ScsiCommand::REZERO, [this]
        {
            CheckReady();
            StatusPhase();
        });
    AddCommand(ScsiCommand::FORMAT_UNIT, [this]
        {
            FormatUnit();
        });
    AddCommand(ScsiCommand::REASSIGN_BLOCKS, [this]
        {
            CheckReady();
            StatusPhase();
        });
    AddCommand(ScsiCommand::READ_6, [this]
        {
            Read(RW6);
        });
    AddCommand(ScsiCommand::WRITE_6, [this]
        {
            Write(RW6);
        });
    AddCommand(ScsiCommand::SEEK_6, [this]
        {
            CheckAndGetStartAndCount(SEEK6);
            StatusPhase();
        });
    AddCommand(ScsiCommand::READ_CAPACITY_10, [this]
        {
            ReadCapacity10();
        });
    AddCommand(ScsiCommand::READ_10, [this]
        {
            Read(RW10);
        });
    AddCommand(ScsiCommand::WRITE_10, [this]
        {
            Write(RW10);
        });
    AddCommand(ScsiCommand::READ_LONG_10, [this]
        {
            ReadWriteLong(ValidateBlockAddress(RW10), GetCdbInt16(7), false);
        });
    AddCommand(ScsiCommand::WRITE_LONG_10, [this]
        {
            ReadWriteLong(ValidateBlockAddress(RW10), GetCdbInt16(7), true);
        });
    AddCommand(ScsiCommand::WRITE_LONG_16, [this]
        {
            ReadWriteLong(ValidateBlockAddress(RW16), GetCdbInt16(12), true);
        });
    AddCommand(ScsiCommand::SEEK_10, [this]
        {
            CheckAndGetStartAndCount(SEEK10);
            StatusPhase();
        });
    AddCommand(ScsiCommand::VERIFY_10, [this]
        {
            Verify(RW10);
        });
    AddCommand(ScsiCommand::SYNCHRONIZE_CACHE_10, [this]
        {
            FlushCache();
            StatusPhase();
        });
    AddCommand(ScsiCommand::SYNCHRONIZE_CACHE_SPACE_16, [this]
        {
            FlushCache();
            StatusPhase();
        });
    AddCommand(ScsiCommand::READ_DEFECT_DATA_10, [this]
        {
            ReadDefectData10();
        });
    AddCommand(ScsiCommand::READ_16, [this]
        {
            Read(RW16);
        });
    AddCommand(ScsiCommand::WRITE_16, [this]
        {
            Write(RW16);
        });
    AddCommand(ScsiCommand::VERIFY_16, [this]
        {
            Verify(RW16);
        });
    AddCommand(ScsiCommand::READ_CAPACITY_READ_LONG_16, [this]
        {
            ReadCapacity16_ReadLong16();
        });
    AddCommand(ScsiCommand::READ_FORMAT_CAPACITIES, [this]
        {
            ReadFormatCapacities();
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
        throw IoException("Drive has 0 sectors");
    }

    StorageDevice::ValidateFile();

    if (!SetUpCache()) {
        throw IoException("Can't initialize cache");
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
        cache = make_shared<DiskCache>(path, GetBlockSize(), GetBlockCount());
    }
    else {
        cache = make_shared<LinuxCache>(path, GetBlockSize(), GetBlockCount(),
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

    // FMTDATA is not supported
    if (GetCdbByte(1) & 0x10) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    StatusPhase();
}

void Disk::Read(AccessMode mode)
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

void Disk::Write(AccessMode mode)
{
    CheckWritePreconditions();

    const auto& [valid, start, count] = CheckAndGetStartAndCount(mode);
    WriteVerify(start, count, valid);
}

void Disk::Verify(AccessMode mode)
{
    // A transfer length of 0 is legal
    const auto& [valid, start, count] = CheckAndGetStartAndCount(mode);

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

void Disk::ReadWriteLong(uint64_t sector, uint32_t length, bool write)
{
    if (write) {
        CheckWritePreconditions();
    }

    if (!length) {
        StatusPhase();
        return;
    }

    // There are no other data but the sector contents
    if (length != GetBlockSize()) {
        SetIli();
        SetInformation(length - GetBlockSize());
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
    if (!linux_cache) {
        // FUll READ/WRITE LONG support requires an appropriate caching mode
        FlushCache();
        caching_mode = PbCachingMode::LINUX;
        InitCache(GetFilename());
        linux_cache = static_pointer_cast<LinuxCache>(cache);
        LogDebug(fmt::format("Switched caching mode to '{}'", PbCachingMode_Name(caching_mode)));
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

void Disk::ReadDefectData10() const
{
    const int allocation_length = min(GetCdbInt16(7), 4);

    GetController()->SetCurrentLength(allocation_length);

    // The defect list is empty
    fill_n(GetController()->GetBuffer().begin(), allocation_length, 0);

    DataInPhase(allocation_length);
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
        buf[3] = byte { 1 };
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

int Disk::ReadData(data_in_t buf)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    if (!cache->ReadSectors(buf, next_sector, sector_transfer_count)) {
        throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::READ_ERROR);
    }

    next_sector += sector_transfer_count;

    UpdateReadCount(sector_transfer_count);

    return GetBlockSize() * sector_transfer_count;
}

int Disk::WriteData(cdb_t cdb, data_out_t buf, int l)
{
    assert(next_sector + sector_transfer_count <= GetBlockCount());

    CheckReady();

    const auto command = static_cast<ScsiCommand>(cdb[0]);

    if (command == ScsiCommand::WRITE_LONG_10 || command == ScsiCommand::WRITE_LONG_16) {
        auto linux_cache = dynamic_pointer_cast<LinuxCache>(cache);
        assert(linux_cache);

        if (!linux_cache->WriteLong(buf, next_sector, GetController()->GetChunkSize())) {
            throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::WRITE_FAULT);
        }

        UpdateWriteCount(1);

        return l;
    }

    if ((command != ScsiCommand::VERIFY_10 && command != ScsiCommand::VERIFY_16)
        && !cache->WriteSectors(buf, next_sector, sector_transfer_count)) {
        throw ScsiException(SenseKey::MEDIUM_ERROR, Asc::WRITE_FAULT);
    }

    next_sector += sector_transfer_count;

    UpdateWriteCount(sector_transfer_count);

    return l;
}

void Disk::ReadCapacity10()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::MEDIUM_NOT_PRESENT);
    }

    // If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
    const uint64_t capacity = GetBlockCount() - 1;
    auto &buf = GetController()->GetBuffer();
    SetInt32(buf, 0, static_cast<uint32_t>(capacity > 0xffffffff ? -1 : capacity));
    SetInt32(buf, 4, GetBlockSize());

    DataInPhase(8);
}

void Disk::ReadCapacity16()
{
    CheckReady();

    if (!GetBlockCount()) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::MEDIUM_NOT_PRESENT);
    }

    fill_n(GetController()->GetBuffer().begin(), 32, 0);

    auto &buf = GetController()->GetBuffer();
    SetInt64(buf, 0, GetBlockCount() - 1);
    SetInt32(buf, 8, GetBlockSize());

    DataInPhase(min(32U, GetCdbInt32(10)));
}

void Disk::ReadFormatCapacities()
{
    CheckReady();

    auto &buf = GetController()->GetBuffer();
    SetInt32(buf, 4, static_cast<uint32_t>(GetBlockCount()));
    SetInt32(buf, 8, GetBlockSize());

    int offset = 12;
    if (!IsReadOnly()) {
        // Return the list of default block sizes
        for (const auto size : GetSupportedBlockSizes()) {
            SetInt32(buf, offset, static_cast<uint32_t>(GetBlockSize() * GetBlockCount() / size));
            SetInt32(buf, offset + 4, size);
            offset += 8;
        }
    }

    SetInt32(buf, 0, offset - 4);

    DataInPhase(min(offset, GetCdbInt16(7)));
}

void Disk::ReadCapacity16_ReadLong16()
{
    // The service action determines the actual command
    switch (GetCdbByte(1) & 0x1f) {
    case 0x10:
        ReadCapacity16();
        break;

    case 0x11:
        ReadWriteLong(ValidateBlockAddress(RW16), GetCdbInt16(12), false);
        break;

    default:
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }
}

uint64_t Disk::ValidateBlockAddress(AccessMode mode)
{
    CheckReady();

    // RelAdr is not supported
    if (mode == RW10 && GetCdbByte(1) & 0x01) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const uint64_t sector = mode == RW16 ? GetCdbInt64(2) : GetCdbInt32(2);

    if (sector >= GetBlockCount()) {
        LogTrace(
            fmt::format("Capacity of {0} sector(s) exceeded: Trying to access sector {1}", GetBlockCount(), sector));
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::LBA_OUT_OF_RANGE);
    }

    return sector;
}

void Disk::ChangeBlockSize(uint32_t new_size)
{
    if (new_size != GetBlockSize()) {
        StorageDevice::ChangeBlockSize(new_size);

        FlushCache();
        if (cache) {
            SetUpCache();
        }
    }
}

tuple<bool, uint64_t, uint32_t> Disk::CheckAndGetStartAndCount(AccessMode mode)
{
    CheckReady();

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
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::LBA_OUT_OF_RANGE);
    }

    // Do not process 0 blocks
    return {count || mode == SEEK6 || mode == SEEK10, start, count};
}

vector<PbStatistics> Disk::GetStatistics() const
{
    vector<PbStatistics> statistics = StorageDevice::GetStatistics();

    if (cache) {
        auto s = cache->GetStatistics(*this);
        statistics.insert(statistics.end(), s.begin(), s.end());
    }

    return statistics;
}
