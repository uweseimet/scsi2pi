//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <vector>
#include <spdlog/spdlog.h>
#include "s2pdump_executor.h"

using namespace std;
using namespace spdlog;
using namespace scsi_defs;

void S2pDumpExecutor::TestUnitReady() const
{
    vector<uint8_t> cdb(6);

    phase_executor->Execute(scsi_command::cmd_test_unit_ready, cdb, { }, 0);
}

bool S2pDumpExecutor::Inquiry(span<uint8_t> buffer, bool sasi)
{
    vector<uint8_t> cdb(6);
    cdb[3] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[4] = static_cast<uint8_t>(buffer.size());

    return phase_executor->Execute(scsi_command::cmd_inquiry, cdb, buffer, static_cast<int>(buffer.size()), sasi);
}

pair<uint64_t, uint32_t> S2pDumpExecutor::ReadCapacity()
{
    vector<uint8_t> buffer(14);
    vector<uint8_t> cdb(10);

    if (!phase_executor->Execute(scsi_command::cmd_read_capacity10, cdb, buffer, 8)) {
        return {0, 0};
    }

    uint64_t capacity = GetInt32(buffer);

    int sector_size_offset = 4;

    if (static_cast<int32_t>(capacity) == -1) {
        cdb.resize(16);
        // READ CAPACITY(16), not READ LONG(16)
        cdb[1] = 0x10;

        if (!phase_executor->Execute(scsi_command::cmd_read_capacity16_read_long16, cdb, buffer,
            static_cast<int>(buffer.size()))) {
            return {0, 0};
        }

        capacity = GetInt64(buffer);

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

        return phase_executor->Execute(is_write ? scsi_command::cmd_write6 : scsi_command::cmd_read6, cdb, buffer,
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

        return phase_executor->Execute(is_write ? scsi_command::cmd_write10 : scsi_command::cmd_read10, cdb, buffer,
            length);
    }
}

bool S2pDumpExecutor::ModeSense6(span<uint8_t> buffer)
{
    vector<uint8_t> cdb(6);
    cdb[1] = 0x08;
    cdb[2] = 0x3f;
    cdb[4] = static_cast<uint8_t>(buffer.size());

    return phase_executor->Execute(scsi_command::cmd_mode_sense6, cdb, buffer, static_cast<int>(buffer.size()));
}

void S2pDumpExecutor::SynchronizeCache()
{
    vector<uint8_t> cdb(10);

    phase_executor->Execute(scsi_command::cmd_synchronize_cache10, cdb, { }, 0);
}

set<int> S2pDumpExecutor::ReportLuns()
{
    vector<uint8_t> buffer(512);
    vector<uint8_t> cdb(12);
    cdb[8] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[9] = static_cast<uint8_t>(buffer.size());

    // Assume 8 LUNs in case REPORT LUNS is not available
    if (!phase_executor->Execute(scsi_command::cmd_report_luns, cdb, buffer, static_cast<int>(buffer.size()))) {
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

uint32_t S2pDumpExecutor::GetInt32(span<uint8_t> buf, int offset)
{
    return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16) |
        (static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

uint64_t S2pDumpExecutor::GetInt64(span<uint8_t> buf, int offset)
{
    return (static_cast<uint64_t>(buf[offset]) << 56) | (static_cast<uint64_t>(buf[offset + 1]) << 48) |
        (static_cast<uint64_t>(buf[offset + 2]) << 40) | (static_cast<uint64_t>(buf[offset + 3]) << 32) |
        (static_cast<uint64_t>(buf[offset + 4]) << 24) | (static_cast<uint64_t>(buf[offset + 5]) << 16) |
        (static_cast<uint64_t>(buf[offset + 6]) << 8) | static_cast<uint64_t>(buf[offset + 7]);
}
