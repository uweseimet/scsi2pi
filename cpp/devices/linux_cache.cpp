//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "linux_cache.h"

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
    file.read((char*)buf.data(), length);
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
    file.write((const char*)buf.data(), length);
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

vector<PbStatistics> LinuxCache::GetStatistics(bool is_read_only) const
{
    vector<PbStatistics> statistics;

    PbStatistics s;

    s.set_category(PbStatisticsCategory::CATEGORY_ERROR);

    s.set_key(READ_ERROR_COUNT);
    s.set_value(read_error_count);
    statistics.push_back(s);

    if (!is_read_only) {
        s.set_key(WRITE_ERROR_COUNT);
        s.set_value(write_error_count);
        statistics.push_back(s);
    }

    return statistics;
}
