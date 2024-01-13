//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <cstring>
#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "mode_page_util.h"

using namespace scsi_defs;
using namespace memory_util;

string mode_page_util::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length, int sector_size)
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);
    assert(length >= 0);

    string result;

    // PF
    if (!(cdb[1] & 0x10)) {
        // Vendor-specific parameters (SCSI-1) are not supported.
        // Do not report an error in order to support Apple's HD SC Setup.
        return result;
    }

    // Skip block descriptors
    int offset;
    if (cmd == scsi_command::cmd_mode_select10) {
        offset = 8 + GetInt16(buf, 6);
    }
    else {
        offset = 4 + buf[3];
    }
    length -= offset;

    // According to the specification the pages data are optional
    bool has_valid_page_code = !length;

    // Parse the pages
    while (length > 0) {
        switch (const int page_code = buf[offset]; page_code) {
        // Read-write error recovery page
        case 0x01:
            if (length < 10) {
                throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
            }

            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            has_valid_page_code = true;
            break;

        // Format device page
        case 0x03: {
            if (length < 22) {
                throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
            }

            // With this page the sector size for a subsequent FORMAT can be selected, but only very few
            // drives support this, e.g FUJITSU M2624S
            // We are fine as long as the current sector size remains unchanged
            if (GetInt16(buf, offset + 12) != sector_size) {
                // It is not possible to permanently (e.g. by formatting) change the sector size.
                // The sector size is an externally configurable setting only.
                spdlog::warn(
                    "Configure the sector size with the '-b' command line option or the 'block_size' device property");
                throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
            }

            has_valid_page_code = true;
            break;
        }

        // Verify error recovery page
        case 0x07:
            if (length < 6) {
                throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
            }

            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            has_valid_page_code = true;
            break;

        default:
            // Remember that there was an unsupported page but continue with the remaining pages
            result = fmt::format("Unsupported MODE SELECT page code: ${:02x}", page_code);
            break;
        }

        // Advance to the next page
        if (length < 2) {
            break;
        }
        const int size = buf[offset + 1] + 2;

        length -= size;
        offset += size;
    }

    // Only report an error if none of the referenced pages are supported
    if (!has_valid_page_code) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
    }

    return result;
}

void mode_page_util::EnrichFormatPage(map<int, vector<byte>> &pages, bool changeable, int sector_size)
{
    if (changeable) {
        // The sector size is simulated to be changeable, see the MODE SELECT implementation for details
        SetInt16(pages[3], 12, sector_size);
    }
}

void mode_page_util::AddAppleVendorModePage(map<int, vector<byte>> &pages, bool changeable)
{
    // Page code 48 (30h) - Apple Vendor Mode Page
    // Needed for SCCD for stock Apple driver support
    // Needed for SCHD for stock Apple HD SC Setup
    pages[48] = vector<byte>(30);

    // No changeable area
    if (!changeable) {
        constexpr const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
        memcpy(&pages[48].data()[2], APPLE_DATA, sizeof(APPLE_DATA));
    }
}
