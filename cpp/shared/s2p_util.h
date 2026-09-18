//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <span>
#include <vector>
#include <fmt/ranges.h>

using namespace std;

namespace s2p_util
{

// Separator for compound options like ID:LUN
inline constexpr char COMPONENT_SEPARATOR = ':';

struct StringHash
{
    using is_transparent = void;

    size_t operator()(string_view s) const
    {
        return hash<string_view> { }(s);
    }
};

inline string Join(const auto &collection, const string &separator = ", ")
{
    return fmt::to_string(fmt::join(collection, separator));
}

string GetVersionString();
vector<string> Split(string_view, char, int = numeric_limits<int>::max());
string ToUpper(string_view);
string ToLower(string_view);
string GetLocale();
string GetLine(const string&, istream& = cin);
int ParseAsUnsignedInt(const string&);
string ParseIdAndLun(const string&, int&, int&);
string Banner(string_view);

vector<byte> HexToBytes(string_view);

constexpr int HexToDec(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

string_view Trim(string_view);

using SignalHandlerPtr = void(*)(int);
void SetTerminationHandler(SignalHandlerPtr);

constexpr const char* to_const_char_ptr(span<const uint8_t> bytes)
{
    return static_cast<const char*>(static_cast<const void*>(bytes.data()));
}

constexpr char* to_char_ptr(span<uint8_t> bytes)
{
    return static_cast<char*>(static_cast<void*>(bytes.data()));
}

constexpr const char* to_const_char_ptr(span<byte> bytes)
{
    return static_cast<const char*>(static_cast<const void*>(bytes.data()));
}

constexpr char* to_char_ptr(span<byte> bytes)
{
    return static_cast<char*>(static_cast<void*>(bytes.data()));
}

}
