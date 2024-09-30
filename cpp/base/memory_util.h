//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
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

    return (static_cast<int>(buf[offset]) << 8) | buf[offset + 1];
}

template<typename T> void SetInt16(vector<T>&, int, int);
template<typename T> void SetInt24(vector<T>&, int, int);
template<typename T> void SetInt32(vector<T>&, int, uint32_t);

int GetInt24(span<const int>, int);
int GetSignedInt24(span<const int>, int);
uint32_t GetInt32(span<const int>, int);
uint64_t GetInt64(span<const int>, int);
void SetInt64(vector<uint8_t>&, int, uint64_t);
}
