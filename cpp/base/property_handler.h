//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <vector>
#include <string>

using namespace std;

using property_map = map<string, string, less<>>;

class PropertyHandler
{

public:

    // Non device-specific property keys
    static constexpr string IMAGE_FOLDER = "image_folder";
    static constexpr string LOCALE = "locale";
    static constexpr string LOG_LEVEL = "log_level";
    static constexpr string LOG_PATTERN = "log_pattern";
    static constexpr string MODE_PAGE = "mode_page";
    static constexpr string PORT = "port";
    static constexpr string PROPERTY_FILES = "property_files";
    static constexpr string RESERVED_IDS = "reserved_ids";
    static constexpr string SCAN_DEPTH = "scan_depth";
    static constexpr string TOKEN_FILE = "token_file";

    static PropertyHandler& Instance()
    {
        static PropertyHandler instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    void Init(const string&, const property_map&);

    property_map GetProperties(const string& = "") const;
    string GetProperty(string_view) const;
    void AddProperty(const string &key, string_view value)
    {
        property_cache[key] = value;
    }
    void RemoveProperties(const string&);

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

private:

    PropertyHandler() = default;

    static void ParsePropertyFile(property_map&, const string&, bool);

    property_map property_cache;

    static constexpr string GLOBAL_CONFIGURATION = "/etc/s2p.conf";
    inline static const string LOCAL_CONFIGURATION = ".config/s2p.conf";
};
