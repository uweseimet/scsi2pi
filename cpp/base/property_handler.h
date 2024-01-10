//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <map>
#include "shared/s2p_util.h"

using namespace std;

class PropertyHandler
{

public:

    static PropertyHandler& Instance()
    {
        static PropertyHandler instance;
        return instance;
    }

    string Init(const string&);

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    PropertyHandler() = default;

    vector<string> GetProperties(const string&) const;

    static vector<byte> HexToBytes(const string&);

    unordered_map<string, vector<string>, s2p_util::StringHash, equal_to<>> property_cache;

    inline static const unordered_map<char, uint8_t> HEX_TO_DEC = {
        { '0', 0 }, { '1', 1 }, { '2', 2 }, { '3', 3 }, { '4', 4 }, { '5', 5 }, { '6', 6 }, { '7', 7 }, { '8', 8 },
        { '9', 9 }, { 'a', 10 }, { 'b', 11 }, { 'c', 12 }, { 'd', 13 }, { 'e', 14 }, { 'f', 15 }
    };

    inline static const string DEFAULT_PROPERTIES_FILE = ".config/s2p.properties";
};
