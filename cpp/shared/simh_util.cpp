//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "simh_util.h"

bool simh_util::ReadMetaData(istream &file, SimhMetaData &meta_data)
{
    array<uint8_t, META_DATA_SIZE> data = { };
    file.read((char*)data.data(), data.size());

    if (file.good()) {
        meta_data = FromLittleEndian(data);
    }
    else {
        if (!file.eof()) {
            file.clear();
            return false;
        }

        file.clear();
        meta_data.cls = simh_class::reserved_marker;
        meta_data.value = static_cast<int>(simh_marker::end_of_medium);
    }

    return true;
}

bool simh_util::IsRecord(const SimhMetaData &meta_data)
{
    // Tape mark
    if (meta_data.cls == simh_class::tape_mark_good_data_record) {
        return meta_data.value;
    }

    // Bad data record, not recovered
    if (meta_data.cls == simh_class::bad_data_record && !meta_data.value) {
        return false;
    }

    return meta_data.cls != simh_class::private_marker && meta_data.cls != simh_class::reserved_marker;
}

uint32_t simh_util::Pad(int length)
{
    assert(length >= 0);

    return length + (length % 2 ? 1 : 0);
}

simh_util::SimhMetaData simh_util::FromLittleEndian(span<const uint8_t> value)
{
    assert(value.size() == sizeof(uint32_t));

    const uint32_t data = (static_cast<uint32_t>(value[3]) << 24) | (static_cast<uint32_t>(value[2]) << 16)
        | (static_cast<uint32_t>(value[1]) << 8) | value[0];

    return {static_cast<simh_class>(data >> 28), data & 0x0fffffff};
}

array<uint8_t, 4> simh_util::ToLittleEndian(const SimhMetaData &meta_data)
{
    return {static_cast<uint8_t>(meta_data.value & 0xff), static_cast<uint8_t>((meta_data.value >> 8) & 0xff),
        static_cast<uint8_t>((meta_data.value >> 16) & 0xff),
        static_cast<uint8_t>(((meta_data.value >> 24) & 0x0f) | static_cast<uint8_t>((static_cast<uint32_t>(meta_data.cls) << 4)))};
}
