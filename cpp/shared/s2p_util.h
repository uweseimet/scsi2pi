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
#include <sstream>
#include <vector>
#include "scsi.h"

namespace s2p_util
{

// Separator for compound options like ID:LUN
static constexpr char COMPONENT_SEPARATOR = ':';

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
string ToUpper(const string&);
string ToLower(const string&);
string GetLocale();
string GetLine(const string&);
bool GetAsUnsignedInt(const string&, int&);
string ProcessId(int, const string&, int&, int&);
string Banner(string_view);

string GetExtensionLowerCase(string_view);

string GetScsiLevel(int);

string FormatSenseData(scsi_defs::sense_key, scsi_defs::asc, int = 0);

vector<byte> HexToBytes(const string&);
string FormatBytes(vector<uint8_t>&, int, bool = false);
int HexToDec(char);
}
