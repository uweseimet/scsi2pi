//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// A basic device with mode page support, to be used for subclassing
//
//---------------------------------------------------------------------------

#include <cstddef>
#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "base/property_handler.h"
#include "mode_page_util.h"
#include "mode_page_device.h"

using namespace std;
using namespace scsi_defs;
using namespace memory_util;
using namespace mode_page_util;

bool ModePageDevice::Init(const param_map &params)
{
    PrimaryDevice::Init(params);

    AddCommand(scsi_command::cmd_mode_sense6, [this]
        {
            ModeSense6();
        });
    AddCommand(scsi_command::cmd_mode_sense10, [this]
        {
            ModeSense10();
        });

    if (supports_mode_pages) {
        AddCommand(scsi_command::cmd_mode_select6, [this]
            {
                ModeSelect6();
            });
        AddCommand(scsi_command::cmd_mode_select10, [this]
            {
                ModeSelect10();
            });
    }

    return true;
}

int ModePageDevice::AddModePages(cdb_t cdb, vector<uint8_t> &buf, int offset, int length, int max_size) const
{
    const int max_length = length - offset;
    if (max_length < 0) {
        return length;
    }

    const bool changeable = (cdb[2] & 0xc0) == 0x40;

    const int page_code = cdb[2] & 0x3f;

    if (page_code == 0x3f) {
        LogTrace("Requesting all mode pages");
    }
    else {
        LogTrace(fmt::format("Requesting mode page ${:02x}", page_code));
    }

    // Mode page data mapped to the respective page numbers, C++ maps are ordered by key
    map<int, vector<byte>> pages;
    SetUpModePages(pages, page_code, changeable);
    for (const auto& [p, data] : property_handler.GetCustomModePages(GetVendor(), GetProduct())) {
        if (data.empty()) {
            pages.erase(p);
        }
        else {
            pages[p] = data;
        }
    }

    if (pages.empty()) {
        LogTrace(fmt::format("Unsupported mode page ${:02x}", page_code));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    // Holds all mode page data
    vector<byte> result;

    vector<byte> page0;
    for (const auto& [index, data] : pages) {
        // The specification mandates that page 0 must be returned after all others
        if (index) {
            const size_t off = result.size();

            // Page data
            result.insert(result.end(), data.begin(), data.end());
            // Page code, PS bit may already have been set
            result[off] |= (byte)index;
            // Page payload size
            result[off + 1] = (byte)(data.size() - 2);
        }
        else {
            page0 = data;
        }
    }

    // Page 0 must be last
    if (!page0.empty()) {
        const size_t off = result.size();

        // Page data
        result.insert(result.end(), page0.begin(), page0.end());
        // Page payload size
        result[off + 1] = (byte)(page0.size() - 2);
    }

    if (static_cast<int>(result.size()) > max_size) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const auto size = static_cast<int>(min(static_cast<size_t>(max_length), result.size()));
    memcpy(&buf.data()[offset], result.data(), size);

    // Do not return more than the requested number of bytes
    return size + offset < length ? size + offset : length;
}

void ModePageDevice::ModeSense6() const
{
    GetController()->SetLength(ModeSense6(GetController()->GetCmd(), GetController()->GetBuffer()));

    EnterDataInPhase();
}

void ModePageDevice::ModeSense10() const
{
    GetController()->SetLength(ModeSense10(GetController()->GetCmd(), GetController()->GetBuffer()));

    EnterDataInPhase();
}

void ModePageDevice::ModeSelect(scsi_command, cdb_t, span<const uint8_t>, int) const
{
    // There is no default implementation of MODE SELECT
    assert(false);

    throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
}

void ModePageDevice::ModeSelect6() const
{
    SaveParametersCheck(GetController()->GetCmdByte(4));
}

void ModePageDevice::ModeSelect10() const
{
    const auto length = min(GetController()->GetBuffer().size(),
        static_cast<size_t>(GetInt16(GetController()->GetCmd(), 7)));

    SaveParametersCheck(static_cast<uint32_t>(length));
}

void ModePageDevice::SaveParametersCheck(int length) const
{
    if (!SupportsSaveParameters() && (GetController()->GetCmdByte(1) & 0x01)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    GetController()->SetLength(length);

    EnterDataOutPhase();
}
