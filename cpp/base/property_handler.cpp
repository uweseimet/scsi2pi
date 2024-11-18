//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "property_handler.h"
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace spdlog;
using namespace s2p_util;

void PropertyHandler::Init(const string &filenames, const property_map &cmd_properties, bool ignore_conf)
{
    // A clear property cache helps with unit testing because Init() can be called for different files
    property_cache.clear();
    unknown_properties.clear();

    property_map properties;

    // Parse the optional global property file unless disabled
    if (!ignore_conf && exists(path(CONFIGURATION))) {
        ParsePropertyFile(properties, CONFIGURATION, true);
    }

    for (const auto &filename : Split(filenames, ',')) {
        ParsePropertyFile(properties, filename, false);
    }

    // Merge properties from property files and from the command line, giving the command line priority
    for (const auto& [key, value] : cmd_properties) {
        properties[key] = value;
    }

    // Normalize properties by adding an explicit LUN where required
    for (const auto& [key, value] : properties) {
        const auto &components = Split(key, '.');
        if (key.starts_with("device.") && key.find(":") == string::npos && components.size() == 3) {
            AddProperty(components[0] + "." + components[1] + ":0." + components[2], value);
        }
        else {
            AddProperty(key, value);
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

property_map PropertyHandler::GetUnknownProperties() const
{
    return unknown_properties;
}

const string& PropertyHandler::RemoveProperty(const string &key, const string &def)
{
    for (const auto& [k, v] : property_cache) {
        if (k == key) {
            unknown_properties.erase(key);
            return v;
        }
    }

    return def;
}

void PropertyHandler::AddProperty(const string &key, string_view value)
{
    property_cache[key] = value;
    unknown_properties[key] = value;
}

void PropertyHandler::RemoveProperties(const string &filter)
{
    erase_if(unknown_properties, [&filter](auto &kv) {return kv.first.starts_with(filter);});
}

bool PropertyHandler::Persist() const
{
    error_code error;
    remove(CONFIGURATION_OLD, error);
    rename(path(CONFIGURATION), path(CONFIGURATION_OLD), error);

    ofstream out(CONFIGURATION);
    for (const auto& [key, value] : GetProperties()) {
        out << key << "=" << value << "\n";
        if (out.fail()) {
            return false;
        }
    }

    return true;
}

