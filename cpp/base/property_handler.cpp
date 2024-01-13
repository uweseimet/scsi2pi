//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <spdlog/spdlog.h>
#include "shared/shared_exceptions.h"
#include "shared/s2p_util.h"
#include "property_handler.h"

using namespace std;
using namespace s2p_util;

void PropertyHandler::Init(const string &f, const property_map &cmd_properties)
{
    // Clearing the property cache helps with testing because Init() can be called for different files
    property_cache.clear();
    property_cache[PropertyHandler::LOCALE] = GetLocale();
    property_cache[PropertyHandler::LOG_LEVEL] = "info";
    property_cache[PropertyHandler::RESERVED_IDS] = "";
    property_cache[PropertyHandler::PORT] = "6868";
    property_cache[PropertyHandler::SCAN_DEPTH] = "1";
    property_cache[PropertyHandler::TOKEN_FILE] = "";

    const string filename = f.empty() ? GetHomeDir() + "/" + DEFAULT_PROPERTIES_FILE : f;

    ifstream property_file(filename);
    // Only report an error if an explicitly specified file is missing
    if (property_file.fail() && !f.empty()) {
        throw parser_exception(fmt::format("No properties file '{}'", filename));
    }

    string property;
    while (getline(property_file, property)) {
        if (property_file.fail()) {
            throw parser_exception(fmt::format("Error reading from properties file '{}'", filename));
        }

        if (!property.empty() && !property.starts_with("#")) {
            const vector<string> kv = Split(property, '=', 2);
            if (kv.size() < 2) {
                throw parser_exception(fmt::format("Invalid property '{}'", property));
            }

            property_cache[kv[0]] = kv[1];
        }
    }

    // Merge properties from property file and from command line
    for (const auto& [k, v] : cmd_properties) {
        property_cache[k] = v;
    }
}

string PropertyHandler::GetProperty(const string &key) const
{
    for (const auto& [k, v] : property_cache) {
        if (k == key) {
            return v;
        }
    }

    return "";
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
            spdlog::warn(fmt::format("Ignored invalid mode page property '{}'", key));
            continue;
        }

        const string identifier = vendor + ":" + product;
        if (!identifier.starts_with(key_components[2])) {
            continue;
        }

        vector<byte> data;
        try {
            data = HexToBytes(value);
        }
        catch (const parser_exception&) {
            spdlog::warn(fmt::format("Ignored invalid mode page definition for page {0}: {1}", page, value));
            continue;
        }

        if (data.empty()) {
            spdlog::trace(fmt::format("Removing default mode page {}", page));
        }
        else {
            // Validate the page code and (except for page 0, which has no well-defined format) the page size
            if (page != (static_cast<int>(data[0]) & 0x3f)) {
                spdlog::warn(
                    fmt::format("Ignored mode page definition with inconsistent page codes {0}: {1}", page, data[0]));
                continue;

            }

            if (page && static_cast<byte>(data.size() - 2) != data[1]) {
                spdlog::warn(
                    fmt::format("Ignored mode page definition with wrong page size {0}: {1}", page, data[1]));
                continue;
            }

            spdlog::trace(fmt::format("Adding/replacing mode page {0}: {1}", page, key_components[2]));
        }

        pages[page] = data;
    }

    return pages;
}

vector<byte> PropertyHandler::HexToBytes(const string &data)
{
    vector<byte> bytes;

    string data_lower;
    ranges::transform(data, back_inserter(data_lower), ::tolower);

    for (size_t i = 0; i < data_lower.length(); i += 2) {
        if (data_lower[i] == ':' && i + 2 < data_lower.length()) {
            i++;
        }

        try {
            bytes.push_back(
                static_cast<byte>(((HEX_TO_DEC.at(data_lower[i]) << 4) + HEX_TO_DEC.at(data_lower[i + 1]))));
        }
        catch (const out_of_range&) {
            throw parser_exception("");
        }
    }

    return bytes;
}
