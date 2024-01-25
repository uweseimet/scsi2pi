//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cstring>
#include <spdlog/spdlog.h>
#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "mode_page_util.h"

using namespace spdlog;
using namespace scsi_defs;
using namespace memory_util;

void mode_page_util::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length, int sector_size)
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    // PF
    if (!(cdb[1] & 0x10)) {
        // Vendor-specific parameters (SCSI-1) are not supported.
        // Do not report an error in order to support Apple's HD SC Setup.
        return;
    }

    // The page data are optional
    if (!length) {
        return;
    }

    int offset = EvaluateBlockDescriptors(cmd, buf, length, sector_size);
    length -= offset;

    // Parse the pages
    while (length > 0) {
        switch (const int page_code = buf[offset]; page_code) {
        // Read-write error recovery page
        case 0x01:
            if (length < 10) {
                throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
            }

            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
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
                warn(
                    "Sector size change from {} to {} bytes per sector requested. Configure the requested sector size in the s2p settings.",
                    sector_size, GetInt16(buf, offset + 12));
                throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
            }

            break;
        }

        // Verify error recovery page
        case 0x07:
            if (length < 6) {
                throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
            }

            // Simply ignore the requested changes in the error handling, they are not relevant for SCSI2Pi
            break;

        default:
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
            break;
        }

        const int size = buf[offset + 1] + 2;
        length -= size;
        offset += size;
    }
}

int mode_page_util::EvaluateBlockDescriptors(scsi_command cmd, span<const uint8_t> buf, int length, int sector_size)
{
    // Check for temporary sector size change in first block descriptor
    int offset;
    if (cmd == scsi_command::cmd_mode_select10) {
        if (length < 8) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        const int block_descriptor_length = GetInt16(buf, 6);
        if (length < block_descriptor_length + 8) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        if (block_descriptor_length && length >= 16 && GetInt16(buf, 14) != sector_size) {
            warn(
                "Sector size change from {} to {} bytes per sector requested. Configure the requested sector size in the s2p settings.",
                sector_size, GetInt16(buf, 14));
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        offset = 8 + block_descriptor_length;
    }
    else {
        if (length < 4) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        const int block_descriptor_length = buf[3];
        if (length < block_descriptor_length + 4) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        if (block_descriptor_length && length >= 12 && GetInt16(buf, 10) != sector_size) {
            warn(
                "Sector size change from {} to {} bytes per sector requested. Configure the requested sector size in the s2p settings.",
                sector_size, GetInt16(buf, 10));
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        offset = 4 + block_descriptor_length;
    }

    return offset;
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
    // Needed for SCCD for stock Apple driver support and stock Apple HD SC Setup
    pages[48] = vector<byte>(24);

    // No changeable area
    if (!changeable) {
        constexpr const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
        memcpy(&pages[48][2], APPLE_DATA, sizeof(APPLE_DATA) - 1);
    }
}
