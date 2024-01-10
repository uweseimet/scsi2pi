//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <spdlog/spdlog.h>
#include "property_handler.h"

using namespace std;
using namespace s2p_util;

string PropertyHandler::Init(const string &f)
{
    assert(property_cache.empty());

    const string filename = f.empty() ? GetHomeDir() + "/" + DEFAULT_PROPERTIES_FILE : f;

    ifstream property_file(filename);
    if (property_file.fail()) {
        // Only report an error if an explicitly specified file is missing
        return f.empty() ? "" : fmt::format("No properties file '{}'", filename);
    }

    string property;
    while (getline(property_file, property)) {
        if (property_file.fail()) {
            return fmt::format("Error reading from properties file '{}'", filename);
        }

        if (!property.starts_with("#")) {
            if (const vector<string> kv = Split(property, '.', 2); kv.size() > 1) {
                auto it = property_cache.find(kv[0]);
                if (it == property_cache.end()) {
                    property_cache[kv[0]] = vector<string>();
                    it = property_cache.find(kv[0]);
                }

                it->second.push_back(kv[1]);
            }
        }
    }

    for (const auto& [key, values] : property_cache) {
        for (const auto &value : values) {
            spdlog::trace(fmt::format("Cached property '{0}.{1}'", key, value));
        }
    }

    return "";
}

vector<string> PropertyHandler::GetProperties(const string &key) const
{
    const auto &it = property_cache.find(key);
    if (it != property_cache.end()) {
        return it->second;
    }

    return {};
}

map<int, vector<byte>> PropertyHandler::GetCustomModePages(const string &vendor, const string &product) const
{
    map<int, vector<byte>> pages;

    for (const auto &mode_page_def : GetProperties("mode_page")) {
        const auto &page_and_remaining = Split(mode_page_def, '.', 2);
        int page;
        if (!GetAsUnsignedInt(page_and_remaining[0], page)) {
            spdlog::error(fmt::format("Ignored invalid mode page data '{}'", mode_page_def));
            continue;
        }

        const auto &identifier_and_data = Split(page_and_remaining[1], '=', 2);

        const string identifier = vendor + ":" + product;
        if (!identifier.starts_with(identifier_and_data[0])) {
            continue;
        }

        vector<byte> data = HexToBytes(identifier_and_data[1]);
        if (data.empty()) {
            spdlog::trace(fmt::format("Removing default mode page {}", page));
        }
        else {
            spdlog::trace(fmt::format("Adding/replacing mode page {0}: {1}", page, identifier_and_data[1]));

            data.insert(data.begin(), static_cast<byte>(page));
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
        try {
            bytes.push_back(
                static_cast<byte>(((HEX_TO_DEC.at(data_lower[i]) << 4) + HEX_TO_DEC.at(data_lower[i + 1]))));
        }
        catch (const out_of_range&) {
            return {};
        }
    }

    return bytes;
}
