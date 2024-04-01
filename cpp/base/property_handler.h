//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
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
    inline static const char *IMAGE_FOLDER = "image_folder";
    inline static const char *LOCALE = "locale";
    inline static const char *LOG_LEVEL = "log_level";
    inline static const char *LOG_PATTERN = "log_pattern";
    inline static const char *MODE_PAGE = "mode_page";
    inline static const char *PORT = "port";
    inline static const char *PROPERTY_FILES = "property_files";
    inline static const char *RESERVED_IDS = "reserved_ids";
    inline static const char *SCAN_DEPTH = "scan_depth";
    inline static const char *TOKEN_FILE = "token_file";

    static PropertyHandler& Instance()
    {
        static PropertyHandler instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    void Init(const string&, const property_map&, bool);

    property_map GetProperties(const string& = "") const;
    string GetProperty(string_view) const;
    void AddProperty(const string &key, string_view value)
    {
        property_cache[key] = value;
    }
    void RemoveProperties(const string&);

    map<int, vector<byte>> GetCustomModePages(const string&, const string&) const;

    bool Persist() const;

private:

    PropertyHandler() = default;

    static void ParsePropertyFile(property_map&, const string&, bool);

    property_map property_cache;

    inline static const string GLOBAL_CONFIGURATION = "/etc/s2p.conf";
    inline static const string LOCAL_CONFIGURATION = ".config/s2p.conf";
};
