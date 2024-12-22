//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "board_executor.h"
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;

void BoardExecutor::TestUnitReady(vector<uint8_t> &cdb) const
{
    initiator_executor->Execute(scsi_command::test_unit_ready, cdb, { }, 0, 1, false);
}

int BoardExecutor::RequestSense(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(scsi_command::request_sense, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::Inquiry(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !initiator_executor->Execute(scsi_command::inquiry, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::ModeSense6(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !initiator_executor->Execute(scsi_command::mode_sense_6, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

set<int> BoardExecutor::ReportLuns(vector<uint8_t> &cdb, span<uint8_t> buf)
{
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

int BoardExecutor::ReadCapacity10(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(scsi_command::read_capacity_10, cdb, buf, 8, 1, true);
}

int BoardExecutor::ReadCapacity16(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(scsi_command::read_capacity_16_read_long_16, cdb, buf, 14, 1, true);
}

bool BoardExecutor::ReadWrite(vector<uint8_t> &cdb, span<uint8_t> buf, int length)
{
    return !initiator_executor->Execute(static_cast<scsi_command>(cdb[0]), cdb, buf, length, 10, true);
}

void BoardExecutor::SynchronizeCache(vector<uint8_t> &cdb) const
{
    initiator_executor->Execute(scsi_command::synchronize_cache_10, cdb, { }, 0, 3, true);
}

int BoardExecutor::Rewind(vector<uint8_t> &cdb) const
{
    return initiator_executor->Execute(scsi_command::rewind, cdb, { }, 0, LONG_TIMEOUT, true);
}

void BoardExecutor::SpaceBack(vector<uint8_t> &cdb) const
{
    if (initiator_executor->Execute(scsi_command::space_6, cdb, { }, 0, LONG_TIMEOUT, false)) {
        throw io_exception("Can't space back one block");
    }
}

int BoardExecutor::WriteFilemark(vector<uint8_t> &cdb) const
{
    return initiator_executor->Execute(scsi_command::write_filemarks_6, cdb, { }, 0, LONG_TIMEOUT, true);
}

int BoardExecutor::Read(vector<uint8_t> &cdb, span<uint8_t> buf, int length, int timeout)
{
    return initiator_executor->Execute(scsi_command::read_6, cdb, buf, length, timeout, false);
}

int BoardExecutor::Write(vector<uint8_t> &cdb, span<uint8_t> buf, int length, int timeout)
{
    if (initiator_executor->Execute(scsi_command::write_6, cdb, buf, length, timeout, false)) {
        throw io_exception(fmt::format("Can't write block with {} byte(s)", length));
    }

    return length;
}
