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

using namespace std;

using property_map = map<string, string>;

class PropertyHandler
{

public:

    // Supported property keys
    inline static const string IMAGE_FOLDER = "image_folder";
    inline static const string LOCALE = "locale";
    inline static const string LOG_LEVEL = "log_level";
    inline static const string MODE_PAGE = "mode_page";
    inline static const string PORT = "port";
    inline static const string PROPERTY_FILES = "property_files";
    inline static const string RESERVED_IDS = "reserved_ids";
    inline static const string SASI = "sasi";
    inline static const string SCAN_DEPTH = "scan_depth";
    inline static const string SCSI = "scsi";
    inline static const string TOKEN_FILE = "token_file";

    static PropertyHandler& Instance()
    {
        static PropertyHandler instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    void Init(const string&, const property_map&);
    property_map GetProperties() const
    {
        return property_cache;
    }
    void ParsePropertyFile(const string&, bool);
    string GetProperty(const string&) const;
    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    PropertyHandler() = default;

    static vector<byte> HexToBytes(const string&);

    property_map property_cache;

    inline static const unordered_map<char, uint8_t> HEX_TO_DEC = {
        { '0', 0 }, { '1', 1 }, { '2', 2 }, { '3', 3 }, { '4', 4 }, { '5', 5 }, { '6', 6 }, { '7', 7 }, { '8', 8 },
        { '9', 9 }, { 'a', 10 }, { 'b', 11 }, { 'c', 12 }, { 'd', 13 }, { 'e', 14 }, { 'f', 15 }
    };

    inline static const string DEFAULT_PROPERTY_FILE = ".config/s2p.properties";
};
