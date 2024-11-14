//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "simh_util.h"

int simh_util::ReadHeader(istream &file, off_t file_size, SimhHeader &header)
{
    if (file.tellg() + HEADER_SIZE > file_size) {
        header.cls = simh_class::reserved_marker;
        header.value = static_cast<int>(simh_marker::end_of_medium);
        return 0;
    }

    array<uint8_t, HEADER_SIZE> h = { };
    file.read((char*)h.data(), h.size());
    if (file.fail()) {
        file.clear();
        header.cls = simh_class::error;
        return 0;
    }

    const uint32_t data = FromLittleEndian(h);
    header.cls = static_cast<simh_class>(data >> 28);
    header.value = data & 0xfffffff;

    return HEADER_SIZE;
}

int simh_util::WriteHeader(ostream &file, off_t file_size, const SimhHeader &header)
{
    if (file.tellp() + HEADER_SIZE > file_size) {
        return OVERFLOW_ERROR;
    }

    file.write((const char*)ToLittleEndian((static_cast<int>(header.cls) << 28) + header.value).data(), HEADER_SIZE);
    file.flush();
    if (file.fail()) {
        file.clear();
        return WRITE_ERROR;
    }

    return HEADER_SIZE;
}

int simh_util::ReadRecord(istream &file, span<uint8_t> buf, int length)
{
    file.read((char*)buf.data(), length);
    if (file.fail()) {
        file.clear();
        return -1;
    }

    return length;
}

int simh_util::WriteRecord(ostream &file, off_t filesize, span<const uint8_t> buf, uint32_t length)
{
    if (static_cast<off_t>(file.tellp()) + Pad(length) + HEADER_SIZE > filesize) {
        return OVERFLOW_ERROR;
    }

    file.write((const char*)buf.data(), length);

    if (length != Pad(length)) {
        file << '\0';
    }

    // Trailing length
    file.write((const char*)ToLittleEndian(length).data(), HEADER_SIZE);

    return Pad(length) + HEADER_SIZE;
}

int64_t simh_util::MoveBack(istream &file)
{
    // Position before trailing length
    file.seekg(-HEADER_SIZE, ios::cur);

    // This is either a trailing length for a data record or a marker
    array<uint8_t, HEADER_SIZE> data = { };
    file.read((char*)data.data(), data.size());
    if (file.fail()) {
        file.clear();
        return -1;
    }

    const uint32_t previous = FromLittleEndian(data);
    const auto cls = static_cast<simh_class>(previous >> 28);
    const uint32_t length = previous & 0xfffffff;

    const int64_t new_position = file.tellg() - (IsRecord(cls) ? Pad(length) + 2 * HEADER_SIZE : HEADER_SIZE);
    if (new_position < 0) {
        return -1;
    }

    file.seekg(new_position, ios::beg);

    return new_position;
}

bool simh_util::IsRecord(simh_class cls)
{
    return cls != simh_class::private_marker && cls != simh_class::reserved_marker && cls != simh_class::error;
}

uint32_t simh_util::Pad(int length)
{
    return length % 2 ? length + 1 : length;
}

uint32_t simh_util::FromLittleEndian(span<const uint8_t> value)
{
    assert(value.size() == sizeof(uint32_t));

    return (static_cast<uint32_t>(value[3]) << 24) | (static_cast<uint32_t>(value[2]) << 16)
        | (static_cast<uint32_t>(value[1]) << 8) | value[0];
}

array<uint8_t, 4> simh_util::ToLittleEndian(uint32_t value)
{
    return {static_cast<uint8_t>(value & 0xff), static_cast<uint8_t>((value >> 8) & 0xff),
        static_cast<uint8_t>((value >> 16) & 0xff), static_cast<uint8_t>((value >> 24) & 0xff)};
}
