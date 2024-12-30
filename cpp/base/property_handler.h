//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>

using namespace std;

using property_map = map<string, string, less<>>;

class PropertyHandler
{

public:

    static constexpr const char *CONFIGURATION = "/etc/s2p.conf";

    // Global property keys
    static constexpr const char *IMAGE_FOLDER = "image_folder";
    static constexpr const char *LOCALE = "locale";
    static constexpr const char *LOG_LEVEL = "log_level";
    static constexpr const char *LOG_LIMIT = "log_limit";
    static constexpr const char *LOG_PATTERN = "log_pattern";
    static constexpr const char *MODE_PAGE = "mode_page";
    static constexpr const char *PORT = "port";
    static constexpr const char *PROPERTY_FILES = "property_files";
    static constexpr const char *RESERVED_IDS = "reserved_ids";
    static constexpr const char *SCAN_DEPTH = "scan_depth";
    static constexpr const char *SCRIPT_FILE = "script_file";
    static constexpr const char *TOKEN_FILE = "token_file";

    // Device-specific property keys
    static constexpr const char *ACTIVE = "active";
    static constexpr const char *TYPE = "type";
    static constexpr const char *SCSI_LEVEL = "scsi_level";
    static constexpr const char *BLOCK_SIZE = "block_size";
    static constexpr const char *CACHING_MODE = "caching_mode";
    static constexpr const char *NAME = "name";
    static constexpr const char *PARAMS = "params";

    static PropertyHandler& Instance()
    {
        static PropertyHandler instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    void Init(const string&, const property_map&, bool);

    property_map GetProperties(const string& = "") const;
    const property_map& GetUnknownProperties() const;
    const string& RemoveProperty(const string&, const string& = "");
    void AddProperty(const string&, string_view);
    void RemoveProperties(const string&);

    bool Persist() const;

private:

    PropertyHandler() = default;

    static void ParsePropertyFile(property_map&, const string&, bool);

    property_map property_cache;

    property_map unknown_properties;

    static constexpr const char *CONFIGURATION_OLD = "/etc/s2p.conf.old";
};
