//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <type_traits>

using namespace std;

namespace memory_util
{

inline int GetInt16(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (static_cast<int>(static_cast<uint8_t>(buf[offset])) << 8) |
        static_cast<int>(static_cast<uint8_t>(buf[offset + 1]));
}

inline int GetInt24(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    return (static_cast<int>(static_cast<uint8_t>(buf[offset])) << 16) |
        (static_cast<int>(static_cast<uint8_t>(buf[offset + 1])) << 8) |
        static_cast<int>(static_cast<uint8_t>(buf[offset + 2]));
}

inline uint32_t GetInt32(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    return (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset])) << 24) |
        (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 1])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 2])) << 8) |
        static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 3]));
}

inline uint64_t GetInt64(const auto &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    return (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset])) << 56) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 1])) << 48) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 2])) << 40) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 3])) << 32) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 4])) << 24) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 5])) << 16) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 6])) << 8) |
        static_cast<uint64_t>(static_cast<uint8_t>(buf[offset + 7]));
}
template<typename Container>
void SetInt16(Container &buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    using ByteType = decay_t<decltype(buf[0])>;

    buf[offset] = static_cast<ByteType>(value >> 8);
    buf[offset + 1] = static_cast<ByteType>(value);
}

inline void SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    buf[offset] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> 16);
    buf[offset + 1] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}

template<typename Container>
void SetInt32(Container &buf, int offset, uint32_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    using ByteType = decay_t<decltype(buf[0])>;

    buf[offset] = static_cast<ByteType>(value >> 24);
    buf[offset + 1] = static_cast<ByteType>(value >> 16);
    buf[offset + 2] = static_cast<ByteType>(value >> 8);
    buf[offset + 3] = static_cast<ByteType>(value);
}

inline void SetInt64(span<uint8_t> buf, int offset, uint64_t value)
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

}
