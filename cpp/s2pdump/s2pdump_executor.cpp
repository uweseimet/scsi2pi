//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pdump_executor.h"
#include <spdlog/spdlog.h>
#include "shared/memory_util.h"

using namespace spdlog;
using namespace memory_util;

void S2pDumpExecutor::TestUnitReady() const
{
    vector<uint8_t> cdb(6);

    initiator_executor->Execute(scsi_command::test_unit_ready, cdb, { }, 0);
}

void S2pDumpExecutor::RequestSense() const
{
    array<uint8_t, 14> buf = { };
    array<uint8_t, 6> cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());
    initiator_executor->Execute(scsi_command::request_sense, cdb, buf, static_cast<int>(buf.size()));
}

bool S2pDumpExecutor::Inquiry(span<uint8_t> buffer)
{
    vector<uint8_t> cdb(6);
    cdb[3] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[4] = static_cast<uint8_t>(buffer.size());

    return !initiator_executor->Execute(scsi_command::inquiry, cdb, buffer, static_cast<int>(buffer.size()));
}

pair<uint64_t, uint32_t> S2pDumpExecutor::ReadCapacity()
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

bool S2pDumpExecutor::ReadWrite(span<uint8_t> buffer, uint32_t bstart, uint32_t blength, int length, bool is_write)
{
    if (bstart < 16777216 && blength <= 256) {
        vector<uint8_t> cdb(6);
        cdb[1] = static_cast<uint8_t>(bstart >> 16);
        cdb[2] = static_cast<uint8_t>(bstart >> 8);
        cdb[3] = static_cast<uint8_t>(bstart);
        cdb[4] = static_cast<uint8_t>(blength);

        return !initiator_executor->Execute(is_write ? scsi_command::write_6 : scsi_command::read_6, cdb, buffer,
            length);
    }
    else {
        vector<uint8_t> cdb(10);
        cdb[2] = static_cast<uint8_t>(bstart >> 24);
        cdb[3] = static_cast<uint8_t>(bstart >> 16);
        cdb[4] = static_cast<uint8_t>(bstart >> 8);
        cdb[5] = static_cast<uint8_t>(bstart);
        cdb[7] = static_cast<uint8_t>(blength >> 8);
        cdb[8] = static_cast<uint8_t>(blength);

        return !initiator_executor->Execute(is_write ? scsi_command::write_10 : scsi_command::read_10, cdb,
            buffer,
            length);
    }
}

bool S2pDumpExecutor::ModeSense6(span<uint8_t> buffer)
{
    vector<uint8_t> cdb(6);
    cdb[1] = 0x08;
    cdb[2] = 0x3f;
    cdb[4] = static_cast<uint8_t>(buffer.size());

    return !initiator_executor->Execute(scsi_command::mode_sense_6, cdb, buffer, static_cast<int>(buffer.size()));
}

void S2pDumpExecutor::SynchronizeCache()
{
    vector<uint8_t> cdb(10);

    initiator_executor->Execute(scsi_command::synchronize_cache_10, cdb, { }, 0);
}

set<int> S2pDumpExecutor::ReportLuns()
{
    vector<uint8_t> buffer(512);
    vector<uint8_t> cdb(12);
    cdb[8] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[9] = static_cast<uint8_t>(buffer.size());

    // Assume 8 LUNs in case REPORT LUNS is not available
    if (initiator_executor->Execute(scsi_command::report_luns, cdb, buffer, static_cast<int>(buffer.size()))) {
        trace("Target does not support REPORT LUNS");
        return {0, 1, 2, 3, 4, 5, 6, 7};
    }

    const auto lun_count = (static_cast<size_t>(buffer[2]) << 8) | static_cast<size_t>(buffer[3]) / 8;
    trace("Target reported LUN count of " + to_string(lun_count));

    set<int> luns;
    int offset = 8;
    for (size_t i = 0; i < lun_count && static_cast<size_t>(offset) + 8 < buffer.size(); i++, offset += 8) {
        const uint64_t lun = GetInt64(buffer, offset);
        if (lun < 32) {
            luns.insert(static_cast<int>(lun));
        }
        else {
            trace("Target reported invalid LUN " + to_string(lun));
        }
    }

    return luns;
}
