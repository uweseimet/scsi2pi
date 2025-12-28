//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "simh_util.h"
#include <cassert>

bool simh_util::ReadMetaData(istream &file, SimhMetaData &meta_data)
{
    array<uint8_t, META_DATA_SIZE> data = { };
    file.read(reinterpret_cast<char*>(data.data()), data.size());

    if (file.good()) {
        meta_data = FromLittleEndian(data);
    }
    else {
        if (!file.eof()) {
            file.clear();
            return false;
        }

        file.clear();
        meta_data.cls = SimhClass::RESERVERD_MARKER;
        meta_data.value = static_cast<uint32_t>(SimhMarker::END_OF_MEDIUM);
    }

    return true;
}

bool simh_util::IsRecord(const SimhMetaData &meta_data)
{
    // Tape mark
    if (meta_data.cls == SimhClass::TAPE_MARK_GOOD_DATA_RECORD) {
        return meta_data.value;
    }

    // Bad data record, not recovered
    if (meta_data.cls == SimhClass::BAD_DATA_RECORD && !meta_data.value) {
        return false;
    }

    return meta_data.cls != SimhClass::PRIVATE_MARKER && meta_data.cls != SimhClass::RESERVERD_MARKER;
}

uint32_t simh_util::Pad(int length)
{
    assert(length >= 0);

    return length + (length % 2 ? 1 : 0);
}

bool simh_util::WriteFilemark(ostream &file)
{
    const array<uint8_t, 4> &filemark = { 0, 0, 0, 0 };
    file.write(reinterpret_cast<const char*>(filemark.data()), filemark.size());
    return file.good();
}

bool simh_util::WriteGoodData(ostream &file, span<const uint8_t> data, int length)
{
    const array<uint8_t, 4> good_data = { static_cast<uint8_t>(length & 0xff),
        static_cast<uint8_t>((length >> 8) & 0xff), static_cast<uint8_t>((length >> 16) & 0xff),
        static_cast<uint8_t>((length >> 24) & 0xff) };
    file.write(reinterpret_cast<const char*>(good_data.data()), good_data.size());
    file.write(reinterpret_cast<const char*>(data.data()), length);
    file.write(reinterpret_cast<const char*>(good_data.data()), good_data.size());
    return file.good();
}

simh_util::SimhMetaData simh_util::FromLittleEndian(span<const uint8_t> value)
{
    assert(value.size() == sizeof(uint32_t));

    const uint32_t data = (static_cast<uint32_t>(value[3]) << 24) | (static_cast<uint32_t>(value[2]) << 16)
        | (static_cast<uint32_t>(value[1]) << 8) | value[0];

    return {static_cast<SimhClass>(data >> 28), data & 0x0fffffff};
}

array<uint8_t, 4> simh_util::ToLittleEndian(const SimhMetaData &meta_data)
{
    return {static_cast<uint8_t>(meta_data.value & 0xff), static_cast<uint8_t>((meta_data.value >> 8) & 0xff),
        static_cast<uint8_t>((meta_data.value >> 16) & 0xff),
        static_cast<uint8_t>(((meta_data.value >> 24) & 0x0f) | static_cast<uint8_t>((static_cast<uint32_t>(meta_data.cls) << 4)))};
}
