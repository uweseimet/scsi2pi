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

    initiator_executor->Execute(scsi_command::test_unit_ready, cdb, { }, 0, 1, false);
}

void S2pDumpExecutor::RequestSense() const
{
    array<uint8_t, 14> buf;
    array<uint8_t, 6> cdb = { };
    cdb[4] = static_cast<uint8_t>(buf.size());

    initiator_executor->Execute(scsi_command::request_sense, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool S2pDumpExecutor::Inquiry(span<uint8_t> buf)
{
    vector<uint8_t> cdb(6);
    cdb[4] = static_cast<uint8_t>(buf.size());

    return !initiator_executor->Execute(scsi_command::inquiry, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool S2pDumpExecutor::ModeSense6(span<uint8_t> buf)
{
    vector<uint8_t> cdb(6);
    cdb[1] = 0x08;
    cdb[2] = 0x3f;
    cdb[4] = static_cast<uint8_t>(buf.size());

    return !initiator_executor->Execute(scsi_command::mode_sense_6, cdb, buf, static_cast<int>(buf.size()), 3, false);
}

set<int> S2pDumpExecutor::ReportLuns()
{
    vector<uint8_t> buf(512);
    vector<uint8_t> cdb(12);
    SetInt16(cdb, 8, buf.size());

    // Assume 8 LUNs in case REPORT LUNS is not available
    if (initiator_executor->Execute(scsi_command::report_luns, cdb, buf, static_cast<int>(buf.size()), 1, false)) {
        GetLogger().trace("Target does not support REPORT LUNS");
        return {0, 1, 2, 3, 4, 5, 6, 7};
    }

    const auto lun_count = (static_cast<size_t>(buf[2]) << 8) | static_cast<size_t>(buf[3]) / 8;
    GetLogger().trace("Target reported LUN count of " + to_string(lun_count));

    set<int> luns;
    int offset = 8;
    for (size_t i = 0; i < lun_count && static_cast<size_t>(offset) + 8 < buf.size(); i++, offset += 8) {
        const uint64_t lun = GetInt64(buf, offset);
        if (lun < 32) {
            luns.insert(static_cast<int>(lun));
        }
        else {
            GetLogger().trace("Target reported invalid LUN " + to_string(lun));
        }
    }

    return luns;
}
