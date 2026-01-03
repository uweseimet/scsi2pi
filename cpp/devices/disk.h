//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XMi:
//   Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <tuple>
#include "storage_device.h"

class Cache;

class Disk : public StorageDevice
{

public:

    ~Disk() override = default;

    string SetUp() override;
    void CleanUp() override;

    bool Eject(bool) override;

    int WriteData(cdb_t, data_out_t, int) override;

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

    void FinalizeSetup(string_view);

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

    void ReadDefectData10() const;
    void ReadCapacity10();
    void ReadCapacity16();
    void ReadFormatCapacities();
    void FormatUnit();
    void Read(AccessMode);
    void Write(AccessMode);
    void Verify(AccessMode);
    void ReadCapacity16_ReadLong16();

    void AddVerifyErrorRecoveryPage(map<int, vector<byte>>&, bool) const;
    void AddCachingPage(map<int, vector<byte>>&, bool) const;

    bool SetUpCache();

    void ReadWriteLong(uint64_t, uint32_t, bool);
    void WriteVerify(uint64_t, uint32_t, bool);
    uint64_t ValidateBlockAddress(AccessMode);
    tuple<bool, uint64_t, uint32_t> CheckAndGetStartAndCount(AccessMode);

    shared_ptr<Cache> cache;

    PbCachingMode caching_mode = PbCachingMode::DEFAULT;

    uint64_t next_sector = 0;

    uint32_t sector_transfer_count = 0;

    struct Unit
    {
        uint64_t threshold;
        uint64_t divisor;
        char abbr;
    };

    inline static constexpr array<Unit, 4> UNITS = { {
        { 10'737'418'240'000, 1'099'511'627'776, 'T' },
        { 10'485'760'000, 1'073'741'824, 'G' },
        { 1'048'576, 1'048'576, 'M' },
        { 0, 1014, 'K' }
    } };
};
