//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "disk_executor.h"
#include <spdlog/spdlog.h>
#include "shared/memory_util.h"

using namespace spdlog;
using namespace memory_util;

pair<uint64_t, uint32_t> DiskExecutor::ReadCapacity()
{
    vector<uint8_t> buffer(14);
    vector<uint8_t> cdb(10);

    if (initiator_executor->Execute(scsi_command::read_capacity_10, cdb, buffer, 8)) {
        return {0, 0};
    }

    uint64_t capacity = GetInt32(buffer, 0);

    int sector_size_offset = 4;

    if (static_cast<int32_t>(capacity) == -1) {
        cdb.resize(16);
        // READ CAPACITY(16), not READ LONG(16)
        cdb[1] = 0x10;

        if (initiator_executor->Execute(scsi_command::read_capacity_16_read_long_16, cdb, buffer,
            static_cast<int>(buffer.size()))) {
            return {0, 0};
        }

        capacity = GetInt64(buffer, 0);

        sector_size_offset = 8;
    }

    const uint32_t sector_size = GetInt32(buffer, sector_size_offset);

    return {capacity + 1, sector_size};
}

bool DiskExecutor::ReadWrite(span<uint8_t> buffer, uint32_t bstart, uint32_t blength, int length, bool is_write)
{
    if (bstart < 16777216 && blength <= 256) {
        vector<uint8_t> cdb(6);
        SetInt32(cdb, 1, bstart);

        return !initiator_executor->Execute(is_write ? scsi_command::write_6 : scsi_command::read_6, cdb, buffer,
            length);
    }
    else {
        vector<uint8_t> cdb(10);
        SetInt32(cdb, 2, bstart);
        SetInt16(cdb, 7, blength);

        return !initiator_executor->Execute(is_write ? scsi_command::write_10 : scsi_command::read_10, cdb, buffer,
            length);
    }
}

void DiskExecutor::SynchronizeCache()
{
    vector<uint8_t> cdb(10);

    initiator_executor->Execute(scsi_command::synchronize_cache_10, cdb, { }, 0);
}
