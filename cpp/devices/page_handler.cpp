//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "page_handler.h"
#include "base/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

PageHandler::PageHandler(PrimaryDevice &d, bool m, bool p) : device(d), supports_mode_select(m), supports_save_parameters(
    p)
{
    device.AddCommand(scsi_command::cmd_mode_sense6, [this]
        {
            device.DataInPhase(
                device.ModeSense6(device.GetController()->GetCdb(), device.GetController()->GetBuffer()));
        });
    device.AddCommand(scsi_command::cmd_mode_sense10, [this]
        {
            device.DataInPhase(
                device.ModeSense10(device.GetController()->GetCdb(), device.GetController()->GetBuffer()));
        });

    // Devices that support MODE SENSE must (at least formally) also support MODE SELECT
    device.AddCommand(scsi_command::cmd_mode_select6, [this]
        {
            SaveParametersCheck(device.GetController()->GetCdbByte(4));
        });
    device.AddCommand(scsi_command::cmd_mode_select10, [this]
        {
            SaveParametersCheck(GetInt16(device.GetController()->GetCdb(), 7));
        });
}

int PageHandler::AddModePages(cdb_t cdb, vector<uint8_t> &buf, int offset, int length, int max_size) const
{
    const int max_length = length - offset;
    if (max_length < 0) {
        return length;
    }

    const bool changeable = (cdb[2] & 0xc0) == 0x40;

    const int page_code = cdb[2] & 0x3f;

    // Mode page data mapped to the respective page codes, C++ maps are ordered by key
    map<int, vector<byte>> pages;
    device.SetUpModePages(pages, page_code, changeable);
    for (const auto& [p, data] : property_handler.GetCustomModePages(device.GetVendor(), device.GetProduct())) {
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
            result[off] |= (byte)index;
            // Page payload size, does not count itself and the page code field
            result[off + 1] = (byte)(page_data.size() - 2);
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

    const auto size = static_cast<int>(min(static_cast<size_t>(max_length), result.size()));
    memcpy(&buf.data()[offset], result.data(), size);

    // Do not return more than the requested number of bytes
    return size + offset < length ? size + offset : length;
}

void PageHandler::SaveParametersCheck(int length) const
{
    if (!supports_mode_select || (!supports_save_parameters && (device.GetController()->GetCdbByte(1) & 0x01))) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    device.DataOutPhase(length);
}
