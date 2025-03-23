//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "board_executor.h"
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

void BoardExecutor::TestUnitReady(span<uint8_t> cdb) const
{
    Execute(ScsiCommand::TEST_UNIT_READY, cdb, { }, 0, 1, false);
}

int BoardExecutor::RequestSense(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return Execute(ScsiCommand::REQUEST_SENSE, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::Inquiry(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return !Execute(ScsiCommand::INQUIRY, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::ModeSense6(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return !Execute(ScsiCommand::MODE_SENSE_6, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

set<int> BoardExecutor::ReportLuns(span<uint8_t> cdb, span<uint8_t> buf)
{
    // Assume 8 LUNs in case REPORT LUNS is not available
    if (Execute(ScsiCommand::REPORT_LUNS, cdb, buf, static_cast<int>(buf.size()), 1, false)) {
        GetLogger().trace("Target does not support REPORT LUNS");
        return {0, 1, 2, 3, 4, 5, 6, 7};
    }

    const auto lun_count = (static_cast<size_t>(buf[2]) << 8) | static_cast<size_t>(buf[3]) / 8;
    GetLogger().trace(fmt::format("Target reported LUN count of {}", lun_count));

    set<int> luns;
    int offset = 8;
    for (size_t i = 0; i < lun_count && static_cast<size_t>(offset) + 8 < buf.size(); ++i, offset += 8) {
        const uint64_t lun = GetInt64(buf, offset);
        if (lun < 32) {
            luns.insert(static_cast<int>(lun));
        }
        else {
            GetLogger().trace(fmt::format("Target reported invalid LUN {}", lun));
        }
    }

    return luns;
}

int BoardExecutor::ReadCapacity10(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return Execute(ScsiCommand::READ_CAPACITY_10, cdb, buf, 8, 1, true);
}

int BoardExecutor::ReadCapacity16(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return Execute(ScsiCommand::READ_CAPACITY_READ_LONG_16, cdb, buf, 14, 1, true);
}

bool BoardExecutor::ReadWrite(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return !Execute(static_cast<ScsiCommand>(cdb[0]), cdb, buf, length, 10, true);
}

void BoardExecutor::SynchronizeCache(span<uint8_t> cdb) const
{
    Execute(ScsiCommand::SYNCHRONIZE_CACHE_10, cdb, { }, 0, 3, true);
}

int BoardExecutor::Rewind(span<uint8_t> cdb) const
{
    return Execute(ScsiCommand::REWIND, cdb, { }, 0, LONG_TIMEOUT, true);
}

void BoardExecutor::SpaceBack(span<uint8_t> cdb) const
{
    if (Execute(ScsiCommand::SPACE_6, cdb, { }, 0, LONG_TIMEOUT, false)) {
        throw IoException("Can't space back one block");
    }
}

int BoardExecutor::WriteFilemark(span<uint8_t> cdb) const
{
    return Execute(ScsiCommand::WRITE_FILEMARKS_6, cdb, { }, 0, LONG_TIMEOUT, true);
}

bool BoardExecutor::Read(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return Execute(ScsiCommand::READ_6, cdb, buf, length, LONG_TIMEOUT, false);
}

bool BoardExecutor::Write(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return Execute(ScsiCommand::WRITE_6, cdb, buf, length, LONG_TIMEOUT, false);
}

bool BoardExecutor::Execute(ScsiCommand cmd, span<uint8_t> cdb, span<uint8_t> buf, int length, int timeout,
    bool enable_log) const
{
    cdb[0] = static_cast<uint8_t>(cmd);
    return initiator_executor->Execute(cdb, buf, length, timeout, enable_log);
}
