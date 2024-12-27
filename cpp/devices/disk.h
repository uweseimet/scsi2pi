//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
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
#include "storage_device.h"

using namespace std;

class Cache;

class Disk : public StorageDevice
{

public:

    ~Disk() override = default;

    string SetUp() override;
    void CleanUp() override;

    bool Eject(bool) override;

    int WriteData(cdb_t, data_out_t, int, int) override;

    int ReadData(data_in_t) override;

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

    Disk(PbDeviceType, int, bool, bool, const set<uint32_t>&);

    void ValidateFile() override;

    bool InitCache(const string&);

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
    void AddAppleVendorPage(map<int, vector<byte>>&, bool) const;

    void ChangeBlockSize(uint32_t) override;

    uint64_t GetNextSector() const
    {
        return next_sector;
    }

private:

    enum AccessMode
    {
        RW6, RW10, RW16, SEEK6, SEEK10
    };

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void SynchronizeCache();
    void ReadDefectData10() const;
    void ReAssignBlocks();
    void ReadCapacity10();
    void ReadCapacity16();
    void ReadFormatCapacities();
    void FormatUnit();
    void Read(AccessMode);
    void Write(AccessMode);
    void Verify(AccessMode);
    void Seek(AccessMode);
    void ReadLong10();
    void ReadLong16();
    void WriteLong10();
    void WriteLong16();
    void ReadCapacity16_ReadLong16();

    void AddVerifyErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddCachingPage(map<int, vector<byte>>&, bool) const;

    bool SetUpCache();

    void ReadWriteLong(uint64_t, uint32_t, bool);
    void WriteVerify(uint64_t, uint32_t, bool);
    uint64_t ValidateBlockAddress(AccessMode) const;
    tuple<bool, uint64_t, uint32_t> CheckAndGetStartAndCount(AccessMode) const;

    shared_ptr<Cache> cache;

    PbCachingMode caching_mode = PbCachingMode::DEFAULT;

    uint64_t next_sector = 0;

    uint32_t sector_transfer_count = 0;
};
