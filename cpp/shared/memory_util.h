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
#include <vector>

using namespace std;

namespace memory_util
{
int GetInt16(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (static_cast<uint32_t>(buf[offset]) << 8) | static_cast<uint32_t>(buf[offset + 1]);
}

int GetInt24(span<const int>, int);
int32_t GetSignedInt24(span<const int>, int);
uint32_t GetInt32(span<const int>, int);
uint64_t GetInt64(span<const int>, int);
uint64_t GetInt64(span<const uint8_t>, int);

void SetInt16(span<byte>, int, int);
void SetInt16(span<uint8_t>, int, int);
void SetInt32(span<byte>, int, uint32_t);
void SetInt32(span<uint8_t>, int, uint32_t);
void SetInt64(span<uint8_t>, int, uint64_t);
}
