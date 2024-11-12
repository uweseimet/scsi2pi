//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "simh_util.h"

pair<int, int> simh_util::ReadHeader(istream &file, int64_t &position)
{
    file.seekg(position, ios::beg);

    uint32_t header;
    file.read((char*)&header, HEADER_SIZE);
    if (file.fail()) {
        file.clear();
        return {-1, -1};
    }

    // TODO Ensure little endian also on big endian platforms
    int cls = header >> 28;
    int value = header & 0x0fffffff;

    position += HEADER_SIZE;

    return {cls, value};
}

int simh_util::WriteHeader(ostream &file, int64_t position, off_t file_size, uint32_t cls, uint32_t value)
{
    if (position + HEADER_SIZE > file_size) {
        return OVERFLOW_ERROR;
    }

    file.seekp(position, ios::beg);

    // TODO Ensure little endian also on big endian platforms
    const uint32_t header = (cls << 28) + value;

    file.write((const char*)&header, HEADER_SIZE);
    file.flush();
    if (file.fail()) {
        file.clear();
        return WRITE_ERROR;
    }

    return 0;
}

int64_t simh_util::MoveBack(istream &file, int64_t position)
{
    assert(position >= static_cast<int64_t>(HEADER_SIZE));

    file.seekg(position - HEADER_SIZE, ios::beg);

    // This is either a trailing length for a data record or a marker
    uint32_t previous;
    file.read((char*)&previous, HEADER_SIZE);

    // TODO Ensure little endian also on big endian platforms
    const uint32_t cls = previous >> 28;
    const uint32_t value = previous & 0x0fffffff;

    // The previous object is a data record, skip the actual data and the two length fields
    if (!cls) {
        if (position < value + 2 * HEADER_SIZE) {
            return -1;
        }

        position -= value + 2 * HEADER_SIZE;
    }
    // The previous object is a marker, skip it
    else {
        position -= HEADER_SIZE;
    }

    return position;
}
