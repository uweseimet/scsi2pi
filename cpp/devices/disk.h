//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XMi:
//   Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <tuple>
#include "base/interfaces/scsi_block_commands.h"
#include "storage_device.h"

using namespace std;

class Cache;

class Disk : public StorageDevice, public ScsiBlockCommands
{

public:

    ~Disk() override = default;

    bool Init(const param_map&) override;
    void CleanUp() override;

    void Dispatch(scsi_command) override;

    bool Eject(bool) override;

    int WriteData(span<const uint8_t>, scsi_command) override;

    int ReadData(span<uint8_t>) override;

    uint32_t GetSectorSizeInBytes() const
    {
        return sector_size;
    }
    bool IsSectorSizeConfigurable() const
    {
        return supported_sector_sizes.size() > 1;
    }
    const auto& GetSupportedSectorSizes() const
    {
        return supported_sector_sizes;
    }
    uint32_t GetConfiguredSectorSize() const
    {
        return configured_sector_size;
    }
    bool SetConfiguredSectorSize(uint32_t);

    PbCachingMode GetCachingMode() const
    {
        return caching_mode;
    }
    void SetCachingMode(PbCachingMode mode)
    {
        caching_mode = mode;
    }
    void FlushCache() override;

    vector<PbStatistics> GetStatistics() const override;

protected:

    Disk(PbDeviceType type, scsi_level level, int lun, bool supports_mode_select, bool supports_save_parameters,
        const unordered_set<uint32_t> &s)
    : StorageDevice(type, level, lun, supports_mode_select, supports_save_parameters), supported_sector_sizes(s)
    {
    }

    void ValidateFile() override;

    bool InitCache(const string&);

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddReadWriteErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddDisconnectReconnectPage(map<int, vector<byte>>&, bool) const;
    void AddVerifyErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddCachingPage(map<int, vector<byte>>&, bool) const;
    void AddControlModePage(map<int, vector<byte>>&, bool) const;
    void AddAppleVendorPage(map<int, vector<byte>>&, bool) const;

    void ModeSelect(cdb_t, span<const uint8_t>, int) override;
    int EvaluateBlockDescriptors(scsi_command, span<const uint8_t>, int&) const;
    int VerifySectorSizeChange(int, bool) const;

    void ChangeSectorSize(uint32_t);
    unordered_set<uint32_t> GetSectorSizes() const;
    bool SetSectorSizeInBytes(uint32_t);

    uint64_t GetNextSector() const
    {
        return next_sector;
    }

private:

    enum access_mode
    {
        RW6, RW10, RW16, SEEK6, SEEK10
    };

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void StartStopUnit();
    void PreventAllowMediumRemoval();
    void SynchronizeCache();
    void ReadDefectData10() const;
    virtual void Read6()
    {
        Read(RW6);
    }
    void Read10() override
    {
        Read(RW10);
    }
    void Read16() override
    {
        Read(RW16);
    }
    virtual void Write6()
    {
        Write(RW6);
    }
    void Write10() override
    {
        Write(RW10);
    }
    void Write16() override
    {
        Write(RW16);
    }
    void ReAssignBlocks();
    void Seek10();
    void ReadCapacity10() override;
    void ReadCapacity16() override;
    void FormatUnit() override;
    void Seek6();
    void Read(access_mode);
    void Write(access_mode);
    void Verify(access_mode);
    void ReadLong10();
    void ReadLong16();
    void WriteLong10();
    void WriteLong16();
    void ReadCapacity16_ReadLong16();

    bool SetUpCache();

    void ReadWriteLong(uint64_t, uint32_t, bool);
    void WriteVerify(uint64_t, uint32_t, bool);
    uint64_t ValidateBlockAddress(access_mode) const;
    tuple<bool, uint64_t, uint32_t> CheckAndGetStartAndCount(access_mode) const;

    int ModeSense6(cdb_t, vector<uint8_t>&) const override;
    int ModeSense10(cdb_t, vector<uint8_t>&) const override;

    shared_ptr<Cache> cache;

    PbCachingMode caching_mode = PbCachingMode::DEFAULT;

    unordered_set<uint32_t> supported_sector_sizes;
    uint32_t configured_sector_size = 0;

    uint64_t next_sector = 0;

    uint32_t sector_transfer_count = 0;

    uint32_t sector_size = 0;

    uint64_t sector_read_count = 0;
    uint64_t sector_write_count = 0;

    string last_filename;

    static constexpr const char *SECTOR_READ_COUNT = "sector_read_count";
    static constexpr const char *SECTOR_WRITE_COUNT = "sector_write_count";
};
