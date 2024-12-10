//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstdint>
#include <span>

using namespace std;

namespace memory_util
{

int GetInt16(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (static_cast<int>(buf[offset]) << 8) | static_cast<int>(buf[offset + 1]);
}

int GetInt24(span<const int>, int);

uint32_t GetInt32(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16) |
        (static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

uint64_t GetInt64(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    return (static_cast<uint64_t>(buf[offset]) << 56) | (static_cast<uint64_t>(buf[offset + 1]) << 48) |
        (static_cast<uint64_t>(buf[offset + 2]) << 40) | (static_cast<uint64_t>(buf[offset + 3]) << 32) |
        (static_cast<uint64_t>(buf[offset + 4]) << 24) | (static_cast<uint64_t>(buf[offset + 5]) << 16) |
        (static_cast<uint64_t>(buf[offset + 6]) << 8) | static_cast<uint64_t>(buf[offset + 7]);
}

void SetInt16(span<byte>, int, int);
void SetInt16(span<uint8_t>, int, int);
void SetInt32(span<byte>, int, uint32_t);
void SetInt32(span<uint8_t>, int, uint32_t);
void SetInt64(span<uint8_t>, int, uint64_t);

}
