//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "simh_util.h"
#include <cassert>
#include "s2p_util.h"

using namespace s2p_util;

bool simh_util::ReadMetaData(istream &file, SimhMetaData &meta_data)
{
    array<uint8_t, META_DATA_SIZE> data = { };
    file.read(to_char_ptr(data), data.size());

    if (file.good()) {
        meta_data = FromLittleEndian(data);
        return true;
    }

    if (!file.eof()) {
        file.clear();
        return false;
    }

    file.clear();

    meta_data = { SimhClass::RESERVERD_MARKER, static_cast<uint32_t>(SimhMarker::END_OF_MEDIUM) };

    return true;
}

bool simh_util::IsRecord(const SimhMetaData &meta_data)
{
    switch (meta_data.cls) {
    // Tape mark
    case SimhClass::TAPE_MARK_GOOD_DATA_RECORD:
        return meta_data.value != 0;

        // Bad data record, not recovered
    case SimhClass::BAD_DATA_RECORD:
        return meta_data.value != 0;

    case SimhClass::PRIVATE_MARKER:
    case SimhClass::RESERVERD_MARKER:
        return false;

    default:
        return true;
    }
}

uint32_t simh_util::Pad(int length)
{
    assert(length >= 0);

    return length + (length % 2 ? 1 : 0);
}

bool simh_util::WriteFilemark(ostream &file)
{
    static constexpr array<uint8_t, 4> filemark = { };
    file.write(to_const_char_ptr(filemark), filemark.size());
    return file.good();
}

bool simh_util::WriteGoodData(ostream &file, span<const uint8_t> data, int length)
{
    const auto good_data = ToLittleEndian( { SimhClass::TAPE_MARK_GOOD_DATA_RECORD, static_cast<uint32_t>(length) });

    file.write(to_const_char_ptr(good_data), good_data.size());
    file.write(to_const_char_ptr(data), length);
    file.write(to_const_char_ptr(good_data), good_data.size());

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
