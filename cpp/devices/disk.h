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

    bool Eject(bool) override;

    int WriteData(span<const uint8_t>, scsi_command) override;

    int ReadData(span<uint8_t>) override;

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
    : StorageDevice(type, level, lun, supports_mode_select, supports_save_parameters, s)
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

    shared_ptr<Cache> cache;

    PbCachingMode caching_mode = PbCachingMode::DEFAULT;

    uint64_t next_sector = 0;

    uint32_t sector_transfer_count = 0;
};
