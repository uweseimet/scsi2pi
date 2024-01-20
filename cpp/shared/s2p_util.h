//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <climits>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include "scsi.h"

using namespace std;

namespace s2p_util
{
// Separator for compound options like ID:LUN
static const char COMPONENT_SEPARATOR = ':';

struct StringHash
{
    using is_transparent = void;

    size_t operator()(string_view sv) const
    {
        hash<string_view> hasher;
        return hasher(sv);
    }
};

string Join(const auto &collection, const string_view separator = ", ")
{
    ostringstream s;

    for (const auto &element : collection) {
        if (s.tellp()) {
            s << separator;
        }

        s << element;
    }

    return s.str();
}

string GetVersionString();
string GetHomeDir();
pair<int, int> GetUidAndGid();
vector<string> Split(const string&, char, int = INT_MAX);
string GetLocale();
bool GetAsUnsignedInt(const string&, int&);
string ProcessId(int, int, const string&, int&, int&);
string Banner(string_view, bool = true);

string GetExtensionLowerCase(string_view);

void LogErrno(const string&);

string FormatSenseData(scsi_defs::sense_key, scsi_defs::asc);

vector<byte> HexToBytes(const string&);
string FormatBytes(vector<uint8_t>&, int);

const unordered_map<char, byte> HEX_TO_DEC = {
    { '0', byte { 0 } }, { '1', byte { 1 } }, { '2', byte { 2 } }, { '3', byte { 3 } }, { '4', byte { 4 } }, { '5',
        byte { 5 } }, { '6', byte { 6 } }, { '7', byte { 7 } }, { '8', byte { 8 } },
    { '9', byte { 9 } }, { 'a', byte { 10 } }, { 'b', byte { 11 } }, { 'c', byte { 12 } }, { 'd', byte { 13 } }, { 'e',
        byte { 14 } }, { 'f', byte { 15 } }
};
}
