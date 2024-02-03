//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "null_cache.h"

NullCache::NullCache(const string &f, int size, uint32_t s, bool raw)
: Cache(raw), filename(f), sector_size(size), sectors(s)
{
    assert(sector_size > 0);
    assert(sectors > 0);
}

bool NullCache::Init()
{
    file.open(filename, ios::in | ios::out);
    return !file.fail();
}

bool NullCache::ReadSector(span<uint8_t> buf, uint32_t sector)
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

bool NullCache::WriteSector(span<const uint8_t> buf, uint32_t sector)
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

    return true;
}

bool NullCache::Flush()
{
    file.flush();
    if (file.fail()) {
        ++write_error_count;
        return false;
    }

    return true;
}

vector<PbStatistics> NullCache::GetStatistics(bool is_read_only) const
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
