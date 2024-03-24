//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// A basic device with mode page support, to be used for subclassing
//
//---------------------------------------------------------------------------

#include <cstddef>
#include "shared/shared_exceptions.h"
#include "base/memory_util.h"
#include "mode_page_device.h"

using namespace memory_util;

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

    if (supports_mode_select) {
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
            result.insert(result.end(), data.cbegin(), data.cend());
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
        result.insert(result.end(), page0.cbegin(), page0.cend());
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
    DataInPhase(ModeSense6(GetController()->GetCdb(), GetController()->GetBuffer()));
}

void ModePageDevice::ModeSense10() const
{
    DataInPhase(ModeSense10(GetController()->GetCdb(), GetController()->GetBuffer()));
}

void ModePageDevice::ModeSelect(scsi_command, cdb_t, span<const uint8_t>, int)
{
    // There is no default implementation of MODE SELECT
    assert(false);

    throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
}

void ModePageDevice::ModeSelect6() const
{
    SaveParametersCheck(GetController()->GetCdbByte(4));
}

void ModePageDevice::ModeSelect10() const
{
    SaveParametersCheck(GetInt16(GetController()->GetCdb(), 7));
}

void ModePageDevice::SaveParametersCheck(int length) const
{
    if (!supports_save_parameters && (GetController()->GetCdbByte(1) & 0x01)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    DataOutPhase(length);
}
