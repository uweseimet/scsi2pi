//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "simh_util.h"

int simh_util::ReadHeader(istream &file, SimhHeader &header)
{
    array<uint8_t, HEADER_SIZE> h = { };
    file.read((char*)h.data(), h.size());
    if (file.fail()) {
        if (file.eof()) {
            file.clear();
            header.cls = simh_class::reserved_marker;
            header.value = static_cast<int>(simh_marker::end_of_medium);
            return 0;
        }

        file.clear();
        return -1;
    }

    const uint32_t data = FromLittleEndian(h);
    header.cls = static_cast<simh_class>(data >> 28);
    header.value = data & 0xfffffff;

    return HEADER_SIZE;
}

bool simh_util::IsRecord(simh_class cls)
{
    return cls != simh_class::private_marker && cls != simh_class::reserved_marker;
}

uint32_t simh_util::GetPadding(int length)
{
    assert(length >= 0);

    return length % 2 ? 1 : 0;
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
