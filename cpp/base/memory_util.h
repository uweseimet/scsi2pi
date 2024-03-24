//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <cassert>
#include <span>
#include <vector>

using namespace std;

namespace memory_util
{
int GetInt16(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (static_cast<int>(buf[offset]) << 8) | buf[offset + 1];
}

template<typename T>
void SetInt16(vector<T> &buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    buf[offset] = static_cast<T>(value >> 8);
    buf[offset + 1] = static_cast<T>(value);
}

template<typename T>
void SetInt32(vector<T> &buf, int offset, uint32_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    buf[offset] = static_cast<T>(value >> 24);
    buf[offset + 1] = static_cast<T>(value >> 16);
    buf[offset + 2] = static_cast<T>(value >> 8);
    buf[offset + 3] = static_cast<T>(value);
}

int GetInt24(span<const int>, int);
uint32_t GetInt32(span<const int>, int);
uint64_t GetInt64(span<const int>, int);
void SetInt64(vector<uint8_t>&, int, uint64_t);
}
