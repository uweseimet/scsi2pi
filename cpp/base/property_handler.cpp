//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "shared/shared_exceptions.h"
#include "property_handler.h"

using namespace filesystem;
using namespace spdlog;
using namespace s2p_util;

void PropertyHandler::Init(const string &filenames, const property_map &cmd_properties)
{
    // A clear property cache helps with testing because Init() can be called for different files
    property_cache.clear();

    property_map properties;
    properties[PropertyHandler::LOCALE] = GetLocale();
    properties[PropertyHandler::LOG_LEVEL] = "info";
    properties[PropertyHandler::PORT] = "6868";
    properties[PropertyHandler::SCAN_DEPTH] = "1";

    // Always parse the optional global property file
    if (exists(path(GLOBAL_CONFIGURATION))) {
        ParsePropertyFile(properties, GLOBAL_CONFIGURATION, true);
    }

    // When there is no explicit property file list parse the local property file
    if (filenames.empty()) {
        ParsePropertyFile(properties, GetHomeDir() + "/" + LOCAL_CONFIGURATION, true);
    }
    else {
        for (const auto &filename : Split(filenames, ',')) {
            ParsePropertyFile(properties, filename, false);
        }
    }

    // Merge properties from property files and from the command line
    for (const auto& [k, v] : cmd_properties) {
        properties[k] = v;
    }

    // Normalize properties by adding an explicit LUN where required
    for (const auto& [key, value] : properties) {
        const auto &components = Split(key, '.');
        if (key.starts_with("device.") && key.find(":") == string::npos && components.size() == 3) {
            property_cache[components[0] + "." + components[1] + ":0." + components[2]] = value;
        }
        else {
            property_cache[key] = value;
        }
    }
}

void PropertyHandler::ParsePropertyFile(property_map &properties, const string &filename, bool default_file)
{
    ifstream property_file(filename);
    if (property_file.fail() && !default_file) {
        // Only report an error if an explicitly specified file is missing
        throw parser_exception(fmt::format("No property file '{}'", filename));
    }

    string property;
    while (getline(property_file, property)) {
        if (property_file.fail()) {
            throw parser_exception(fmt::format("Error reading from property file '{}'", filename));
        }

        if (!property.empty() && !property.starts_with("#")) {
            const vector<string> kv = Split(property, '=', 2);
            if (kv.size() < 2) {
                throw parser_exception(fmt::format("Invalid property '{}'", property));
            }

            properties[kv[0]] = kv[1];
        }
    }
}

property_map PropertyHandler::GetProperties(const string &filter) const
{
    if (filter.empty()) {
        return property_cache;
    }

    property_map filtered_properties;
    for (const auto& [key, value] : property_cache) {
        if (key.starts_with(filter)) {
            filtered_properties[key] = value;
        }
    }

    return filtered_properties;
}

string PropertyHandler::GetProperty(string_view key) const
{
    for (const auto& [k, v] : property_cache) {
        if (k == key) {
            return v;
        }
    }

    return "";
}

void PropertyHandler::RemoveProperties(const string &filter)
{
    erase_if(property_cache, [&filter](auto &kv) {return kv.first.starts_with(filter);});
}

map<int, vector<byte>> PropertyHandler::GetCustomModePages(const string &vendor, const string &product) const
{
    map<int, vector<byte>> pages;

    for (const auto& [key, value] : property_cache) {
        const auto &key_components = Split(key, '.', 3);

        if (key_components[0] != MODE_PAGE) {
            continue;
        }

        int page;
        if (!GetAsUnsignedInt(key_components[1], page) || page > 0x3e) {
            warn("Ignored invalid mode page property '{}'", key);
            continue;
        }

        if (const string identifier = vendor + COMPONENT_SEPARATOR + product; !identifier.starts_with(
            key_components[2])) {
            continue;
        }

        vector<byte> data;
        try {
            data = HexToBytes(value);
        }
        catch (const out_of_range&) {
            warn("Ignored invalid mode page definition for page {0}: {1}", page, value);
            continue;
        }

        if (data.empty()) {
            trace("Removing default mode page {}", page);
        }
        else {
            // Validate the page code and (except for page 0, which has no well-defined format) the page size
            if (page != (static_cast<int>(data[0]) & 0x3f)) {
                warn("Ignored mode page definition with inconsistent page codes {0}: {1}", page, data[0]);
                continue;

            }

            if (page && static_cast<byte>(data.size() - 2) != data[1]) {
                warn("Ignored mode page definition with wrong page size {0}: {1}", page, data[1]);
                continue;
            }

            trace("Adding/replacing mode page {0}: {1}", page, key_components[2]);
        }

        pages[page] = data;
    }

    return pages;
}
