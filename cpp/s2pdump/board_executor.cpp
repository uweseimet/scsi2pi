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

void BoardExecutor::TestUnitReady(vector<uint8_t> &cdb) const
{
    initiator_executor->Execute(ScsiCommand::TEST_UNIT_READY, cdb, { }, 0, 1, false);
}

int BoardExecutor::RequestSense(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(ScsiCommand::REQUEST_SENSE, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::Inquiry(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !initiator_executor->Execute(ScsiCommand::INQUIRY, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

bool BoardExecutor::ModeSense6(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !initiator_executor->Execute(ScsiCommand::MODE_SENSE_6, cdb, buf, static_cast<int>(buf.size()), 1, false);
}

set<int> BoardExecutor::ReportLuns(vector<uint8_t> &cdb, span<uint8_t> buf)
{
    // Assume 8 LUNs in case REPORT LUNS is not available
    if (initiator_executor->Execute(ScsiCommand::REPORT_LUNS, cdb, buf, static_cast<int>(buf.size()), 1, false)) {
        GetLogger().trace("Target does not support REPORT LUNS");
        return {0, 1, 2, 3, 4, 5, 6, 7};
    }

    const auto lun_count = (static_cast<size_t>(buf[2]) << 8) | static_cast<size_t>(buf[3]) / 8;
    GetLogger().trace(fmt::format("Target reported LUN count of {}", lun_count));

    set<int> luns;
    int offset = 8;
    for (size_t i = 0; i < lun_count && static_cast<size_t>(offset) + 8 < buf.size(); i++, offset += 8) {
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

int BoardExecutor::ReadCapacity10(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(ScsiCommand::READ_CAPACITY_10, cdb, buf, 8, 1, true);
}

int BoardExecutor::ReadCapacity16(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return initiator_executor->Execute(ScsiCommand::READ_CAPACITY_READ_LONG_16, cdb, buf, 14, 1, true);
}

bool BoardExecutor::ReadWrite(vector<uint8_t> &cdb, span<uint8_t> buf, int length)
{
    return !initiator_executor->Execute(static_cast<ScsiCommand>(cdb[0]), cdb, buf, length, 10, true);
}

void BoardExecutor::SynchronizeCache(vector<uint8_t> &cdb) const
{
    initiator_executor->Execute(ScsiCommand::SYNCHRONIZE_CACHE_10, cdb, { }, 0, 3, true);
}

int BoardExecutor::Rewind(vector<uint8_t> &cdb) const
{
    return initiator_executor->Execute(ScsiCommand::REWIND, cdb, { }, 0, LONG_TIMEOUT, true);
}

void BoardExecutor::SpaceBack(vector<uint8_t> &cdb) const
{
    if (initiator_executor->Execute(ScsiCommand::SPACE_6, cdb, { }, 0, LONG_TIMEOUT, false)) {
        throw IoException("Can't space back one block");
    }
}

int BoardExecutor::WriteFilemark(vector<uint8_t> &cdb) const
{
    return initiator_executor->Execute(ScsiCommand::WRITE_FILEMARKS_6, cdb, { }, 0, LONG_TIMEOUT, true);
}

bool BoardExecutor::Read(vector<uint8_t> &cdb, span<uint8_t> buf, int length)
{
    return initiator_executor->Execute(ScsiCommand::READ_6, cdb, buf, length, LONG_TIMEOUT, false);
}

bool BoardExecutor::Write(vector<uint8_t> &cdb, span<uint8_t> buf, int length)
{
    return initiator_executor->Execute(ScsiCommand::WRITE_6, cdb, buf, length, LONG_TIMEOUT, false);
}
