//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_util.h"
#include "shared/command_meta_data.h"
#include "shared/memory_util.h"

using namespace memory_util;

void sg_util::UpdateStartBlock(span<uint8_t> cdb, int length)
{
    switch (const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])); meta_data.block_size) {
    case 3:
        SetInt24(cdb, meta_data.block_offset, GetInt24(cdb, meta_data.block_offset) + length);
        break;

    case 4:
        SetInt32(cdb, meta_data.block_offset, GetInt32(cdb, meta_data.block_offset) + length);
        break;

    case 8:
        SetInt64(cdb, meta_data.block_offset, GetInt64(cdb, meta_data.block_offset) + length);
        break;

    default:
        break;
    }
}

void sg_util::SetBlockCount(span<uint8_t> cdb, int length)
{
    const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0]));
    if (meta_data.block_size) {
        switch (meta_data.allocation_length_size) {
        case 1:
            cdb[meta_data.allocation_length_offset] = static_cast<uint8_t>(length);
            break;

        case 2:
            SetInt16(cdb, meta_data.allocation_length_offset, length);
            break;

        case 4:
            SetInt32(cdb, meta_data.allocation_length_offset, length);
            break;

        default:
            assert(false);
            break;
        }
    }
}

void sg_util::SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    buf[offset] = static_cast<uint8_t>(value >> 16);
    buf[offset + 1] = static_cast<uint8_t>(value >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}
