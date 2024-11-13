//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "simh_util.h"

pair<simh_util::simh_class, int> simh_util::ReadHeader(istream &file, int64_t &position)
{
    file.seekg(position, ios::beg);

    uint32_t header;
    file.read((char*)&header, HEADER_SIZE);
    if (file.fail()) {
        file.clear();
        return {simh_class::invalid, -1};
    }

    // TODO Ensure little endian also on big endian platforms
    const auto cls = static_cast<simh_class>(header >> 28);
    const int value = header & 0xfffffff;

    position += HEADER_SIZE;

    return {cls, value};
}

int simh_util::WriteHeader(ostream &file, int64_t position, off_t file_size, simh_class cls, uint32_t value)
{
    if (position + HEADER_SIZE > file_size) {
        return OVERFLOW_ERROR;
    }

    file.seekp(position, ios::beg);

    // TODO Ensure little endian also on big endian platforms
    const uint32_t header = (static_cast<int>(cls) << 28) + value;

    file.write((const char*)&header, HEADER_SIZE);
    file.flush();
    if (file.fail()) {
        file.clear();
        return WRITE_ERROR;
    }

    return HEADER_SIZE;
}

int simh_util::WriteRecord(ostream &file, int64_t position, span<const uint8_t> buf, uint32_t length)
{
    file.seekp(position, ios::beg);

    file.write((const char*)buf.data(), length);

    if (length != Pad(length)) {
        file << '\0';
    }

    // Trailing length
    // TODO Ensure little endian also on big endian platforms
    file.write((const char*)&length, HEADER_SIZE);

    return Pad(length) + HEADER_SIZE;
}

int64_t simh_util::MoveBack(istream &file, int64_t position)
{
    // Position before trailing length
    file.seekg(position - HEADER_SIZE, ios::beg);

    // This is either a trailing length for a data record or a marker
    uint32_t previous;
    file.read((char*)&previous, HEADER_SIZE);
    if (file.fail()) {
        file.clear();
        return -1;
    }

    // TODO Ensure little endian also on big endian platforms
    const auto cls = static_cast<simh_class>(previous >> 28);
    const uint32_t length = previous & 0xfffffff;

    const int64_t new_position = position - (IsRecord(cls) ? Pad(length) + 2 * HEADER_SIZE : HEADER_SIZE);
    return new_position >= 0 ? new_position : -1;
}

bool simh_util::IsRecord(simh_class cls)
{
    return cls != simh_class::private_marker && cls != simh_class::reserved_marker && cls != simh_class::invalid;
}

uint32_t simh_util::Pad(int length)
{
    return length % 2 ? length + 1 : length;
}

