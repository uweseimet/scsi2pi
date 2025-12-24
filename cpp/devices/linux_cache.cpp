//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "linux_cache.h"
#include "base/device.h"

bool LinuxCache::Init()
{
    if (!sector_size || !sectors || filename.empty()) {
        return false;
    }

    file.open(filename, ios::in | ios::out | ios::binary);
    return file.good();
}

int LinuxCache::ReadSectors(data_in_t buf, uint64_t start, uint32_t count)
{
    return sectors < start + count ? 0 : Read(buf, start, sector_size * count);
}

int LinuxCache::WriteSectors(data_out_t buf, uint64_t start, uint32_t count)
{
    return sectors < start + count ? 0 : Write(buf, start, sector_size * count);
}

int LinuxCache::ReadLong(data_in_t buf, uint64_t start, int length)
{
    return sectors <= start ? 0 : Read(buf, start, length);
}

int LinuxCache::WriteLong(data_out_t buf, uint64_t start, int length)
{
    return sectors <= start ? 0 : Write(buf, start, length);
}

int LinuxCache::Read(data_in_t buf, uint64_t start, int length)
{
    assert(length);

    file.seekg(sector_size * start);
    file.read(reinterpret_cast<char*>(buf.data()), length);
    if (file.fail()) {
        file.clear();
        ++read_error_count;
        return 0;
    }

    return length;
}

int LinuxCache::Write(data_out_t buf, uint64_t start, int length)
{
    assert(length);

    file.seekp(sector_size * start);
    file.write(reinterpret_cast<const char*>(buf.data()), length);
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        return 0;
    }

    if (write_through) {
        return Flush() ? length : 0;
    }

    return length;
}

bool LinuxCache::Flush()
{
    file.flush();
    if (file.fail()) {
        file.clear();
        ++write_error_count;
        return false;
    }

    return true;
}

vector<PbStatistics> LinuxCache::GetStatistics(const Device &device) const
{
    vector<PbStatistics> statistics;

    device.EnrichStatistics(statistics, CATEGORY_ERROR, READ_ERROR_COUNT, read_error_count);
    if (!device.IsReadOnly()) {
        device.EnrichStatistics(statistics, CATEGORY_ERROR, WRITE_ERROR_COUNT, write_error_count);
    }

    return statistics;
}
