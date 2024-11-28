//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "memory_util.h"

// Note: Templates cannot be used with span<T>, affecting SetInt*

int memory_util::GetInt24(span<const int> buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    return (buf[offset] << 16) | (buf[offset + 1] << 8) | buf[offset + 2];
}

int32_t memory_util::GetSignedInt24(span<const int> buf, int offset)
{
    const int value = GetInt24(buf, offset);

    return value >= 0x800000 ? value - 0x1000000 : value;
}

void memory_util::SetInt16(span<byte> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    buf[offset] = static_cast<byte>(value >> 8);
    buf[offset + 1] = static_cast<byte>(value);
}

void memory_util::SetInt16(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    buf[offset] = static_cast<uint8_t>(value >> 8);
    buf[offset + 1] = static_cast<uint8_t>(value);
}

void memory_util::SetInt32(span<byte> buf, int offset, uint32_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    buf[offset] = static_cast<byte>(value >> 24);
    buf[offset + 1] = static_cast<byte>(value >> 16);
    buf[offset + 2] = static_cast<byte>(value >> 8);
    buf[offset + 3] = static_cast<byte>(value);
}

void memory_util::SetInt32(span<uint8_t> buf, int offset, uint32_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    buf[offset] = static_cast<uint8_t>(value >> 24);
    buf[offset + 1] = static_cast<uint8_t>(value >> 16);
    buf[offset + 2] = static_cast<uint8_t>(value >> 8);
    buf[offset + 3] = static_cast<uint8_t>(value);
}

void memory_util::SetInt64(span<uint8_t> buf, int offset, uint64_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    buf[offset] = static_cast<uint8_t>(value >> 56);
    buf[offset + 1] = static_cast<uint8_t>(value >> 48);
    buf[offset + 2] = static_cast<uint8_t>(value >> 40);
    buf[offset + 3] = static_cast<uint8_t>(value >> 32);
    buf[offset + 4] = static_cast<uint8_t>(value >> 24);
    buf[offset + 5] = static_cast<uint8_t>(value >> 16);
    buf[offset + 6] = static_cast<uint8_t>(value >> 8);
    buf[offset + 7] = static_cast<uint8_t>(value);
}
