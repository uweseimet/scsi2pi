//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "linux_cache.h"

LinuxCache::LinuxCache(const string &f, int size, uint64_t s, bool raw, bool w)
: Cache(raw), filename(f), sector_size(size), sectors(s), write_through(w)
{
    assert(sector_size > 0);
    assert(sectors > 0);
}

bool LinuxCache::Init()
{
    file.open(filename, ios::in | ios::out);
    return !file.fail();
}

bool LinuxCache::ReadSector(span<uint8_t> buf, uint64_t sector)
{
    if (sectors < sector) {
        return false;
    }

    file.seekg(sector_size * sector, ios::beg);
    if (file.fail()) {
        ++read_error_count;
        return false;
    }

    file.read((char*)buf.data(), sector_size);
    if (file.fail()) {
        ++read_error_count;
        return false;
    }

    return true;
}

bool LinuxCache::WriteSector(span<const uint8_t> buf, uint64_t sector)
{
    if (sectors < sector) {
        return false;
    }

    file.seekp(sector_size * sector, ios::beg);
    if (file.fail()) {
        ++write_error_count;
        return false;
    }

    file.write((const char*)buf.data(), sector_size);
    if (file.fail()) {
        ++write_error_count;
        return false;
    }

    if (write_through) {
        file.flush();
        if (file.fail()) {
            ++write_error_count;
            return false;
        }
    }

    return true;
}

int LinuxCache::ReadLong(span<uint8_t> buf, uint64_t sector, int length)
{
    if (sectors < sector) {
        return 0;
    }

    file.seekg(sector_size * sector, ios::beg);
    if (file.fail()) {
        ++read_error_count;
        return 0;
    }

    file.read((char*)buf.data(), length);
    if (file.fail()) {
        ++read_error_count;
        return 0;
    }

    return length;
}

int LinuxCache::WriteLong(span<const uint8_t> buf, uint64_t sector, int length)
{
    if (sectors < sector) {
        return 0;
    }

    file.seekp(sector_size * sector, ios::beg);
    if (file.fail()) {
        ++write_error_count;
        return 0;
    }

    file.write((const char*)buf.data(), length);
    if (file.fail()) {
        ++write_error_count;
        return 0;
    }

    if (write_through) {
        file.flush();
        if (file.fail()) {
            ++write_error_count;
            return 0;
        }
    }

    return length;
}

bool LinuxCache::Flush()
{
    file.flush();
    if (file.fail()) {
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
