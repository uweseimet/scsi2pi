//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include "base/interfaces/scsi_block_commands.h"
#include "shared/s2p_util.h"
#include "disk_track.h"
#include "disk_cache.h"
#include "storage_device.h"

using namespace std;

class Disk : public StorageDevice, private ScsiBlockCommands
{

public:

    Disk(PbDeviceType type, scsi_level level, int lun, bool supports_mode_pages, const unordered_set<uint32_t> &s)
    : StorageDevice(type, level, lun, supports_mode_pages), supported_sector_sizes(s)
    {
    }
    ~Disk() override = default;

    bool Init(const param_map&) override;
    void CleanUp() override;

    void Dispatch(scsi_command) override;

    bool Eject(bool) override;

    virtual void Write(span<const uint8_t>, uint64_t);

    virtual int Read(span<uint8_t>, uint64_t);

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
    void FlushCache() override;

    vector<PbStatistics> GetStatistics() const override;

protected:

    void SetUpCache(bool = false);
    void ResizeCache(const string&, bool);

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddReadWriteErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddDisconnectReconnectPage(map<int, vector<byte>>&, bool) const;
    void AddVerifyErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddCachingPage(map<int, vector<byte>>&, bool) const;
    void AddControlModePage(map<int, vector<byte>>&, bool) const;
    void AddAppleVendorPage(map<int, vector<byte>>&, bool) const;

    void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) override;
    int EvaluateBlockDescriptors(scsi_defs::scsi_command, span<const uint8_t>, int, int&) const;
    int VerifySectorSizeChange(int, bool) const;

    void ChangeSectorSize(uint32_t);
    unordered_set<uint32_t> GetSectorSizes() const;
    bool SetSectorSizeInBytes(uint32_t);
    void SetSectorSizeShiftCount(uint32_t count)
    {
        sector_size = 1 << count;
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
    void Verify10()
    {
        Verify(RW10);
    }
    void Verify16()
    {
        Verify(RW16);
    }
    void Seek();
    void Seek10();
    void ReadCapacity10() override;
    void ReadCapacity16() override;
    void FormatUnit() override;
    void Seek6();
    void Read(access_mode);
    void Write(access_mode) const;
    void Verify(access_mode);
    void ReadWriteLong10() const;
    void ReadWriteLong16() const;
    void ReadCapacity16_read_long16();

    void ValidateBlockAddress(access_mode) const;
    tuple<bool, uint64_t, uint32_t> CheckAndGetStartAndCount(access_mode) const;

    int ModeSense6(cdb_t, vector<uint8_t>&) const override;
    int ModeSense10(cdb_t, vector<uint8_t>&) const override;

    unique_ptr<DiskCache> cache;

    unordered_set<uint32_t> supported_sector_sizes;
    uint32_t configured_sector_size = 0;

    uint32_t sector_size = 0;

    uint64_t sector_read_count = 0;
    uint64_t sector_write_count = 0;

    inline static const string SECTOR_READ_COUNT = "sector_read_count";
    inline static const string SECTOR_WRITE_COUNT = "sector_write_count";
};
