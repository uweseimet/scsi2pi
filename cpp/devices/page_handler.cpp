//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "page_handler.h"
#include "base/property_handler.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;
using namespace s2p_util;

PageHandler::PageHandler(PrimaryDevice &d, bool m, bool p) : device(d), supports_mode_select(m), supports_save_parameters(
    p)
{
    device.AddCommand(scsi_command::mode_sense_6, [this]
        {
            device.DataInPhase(
                device.ModeSense6(device.GetController()->GetCdb(), device.GetController()->GetBuffer()));
        });
    device.AddCommand(scsi_command::mode_sense_10, [this]
        {
            device.DataInPhase(
                device.ModeSense10(device.GetController()->GetCdb(), device.GetController()->GetBuffer()));
        });

    // Devices that support MODE SENSE must (at least formally) also support MODE SELECT
    device.AddCommand(scsi_command::mode_select_6, [this]
        {
            ModeSelect(device.GetCdbByte(4));
        });
    device.AddCommand(scsi_command::mode_select_10, [this]
        {
            ModeSelect(device.GetCdbInt24(7));
        });
}

int PageHandler::AddModePages(cdb_t cdb, data_in_t buf, int offset, int length, int max_size) const
{
    const int max_length = length - offset;
    if (max_length < 0) {
        return length;
    }

    const bool changeable = (cdb[2] & 0xc0) == 0x40;

    const auto page_code = cdb[2] & 0x3f;

    // Mode page data mapped to the respective page codes, C++ maps are ordered by key
    map<int, vector<byte>> pages;
    device.SetUpModePages(pages, page_code, changeable);
    for (const auto& [p, data] : GetCustomModePages(device.GetVendor(), device.GetProduct())) {
        if (data.empty()) {
            pages.erase(p);
        }
        else {
            pages[p] = data;
        }
    }

    if (pages.empty()) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    // Holds all mode page data
    vector<byte> result;

    for (const auto& [index, page_data] : pages) {
        // The specification mandates that page 0 must be returned last
        if (index) {
            const auto off = result.size();

            // Page data
            result.insert(result.end(), page_data.cbegin(), page_data.cend());
            // Page code, PS bit may already have been set
            result[off] |= static_cast<byte>(index);
            // Page payload size, does not count itself and the page code field
            result[off + 1] = static_cast<byte>(page_data.size() - 2);
        }
    }

    if (pages.contains(0)) {
        // Page data only (there is no standardized size field for page 0)
        const auto &page_data = pages[0];
        result.insert(result.end(), page_data.cbegin(), page_data.cend());
    }

    if (static_cast<int>(result.size()) > max_size) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const int size = min(max_length, static_cast<int>(result.size()));
    memcpy(&buf.data()[offset], result.data(), size);

    // Do not return more than the requested number of bytes
    return size + offset < length ? size + offset : length;
}

map<int, vector<byte>> PageHandler::GetCustomModePages(const string &vendor, const string &product) const
{
    map<int, vector<byte>> pages;

    for (const auto& [key, value] : PropertyHandler::Instance().GetProperties()) {
        const auto &key_components = Split(key, '.', 3);

        if (key_components[0] != PropertyHandler::MODE_PAGE) {
            continue;
        }

        int page_code;
        if (!GetAsUnsignedInt(key_components[1], page_code) || page_code > 0x3e) {
            warn("Ignored invalid page code in mode page property '{}'", key);
            continue;
        }

        if (const string identifier = vendor + COMPONENT_SEPARATOR + product; !identifier.starts_with(
            key_components[2])) {
            continue;
        }

        vector<byte> page_data;
        try {
            page_data = HexToBytes(value);
        }
        catch (const out_of_range&) {
            warn("Ignored invalid mode page definition for page {0}: {1}", page_code, value);
            continue;
        }

        if (page_data.empty()) {
            trace("Removing default mode page {}", page_code);
        }
        else {
            // Validate the page code and (except for page 0, which has no well-defined format) the page size
            if (page_code != to_integer<int>(page_data[0] & byte { 0x3f })) {
                warn("Ignored mode page definition with inconsistent page code {0}: {1}", page_code, page_data[0]);
                continue;
            }

            if (page_code && static_cast<byte>(page_data.size() - 2) != page_data[1]) {
                warn("Ignored mode page definition with wrong page size {0}: {1}", page_code, page_data[1]);
                continue;
            }

            trace("Adding/replacing mode page {0}: {1}", page_code, value);
        }

        pages[page_code] = page_data;
    }

    return pages;
}

void PageHandler::ModeSelect(int length) const
{
    if (!supports_mode_select || (!supports_save_parameters && (device.GetCdbByte(1) & 0x01))) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    device.DataOutPhase(length);
}
