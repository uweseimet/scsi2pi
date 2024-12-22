//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_executor.h"
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

void SgExecutor::TestUnitReady(vector<uint8_t> &cdb) const
{
    sg_adapter.SendCommand(cdb, { }, 0, 1);
}

int SgExecutor::RequestSense(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

bool SgExecutor::Inquiry(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

bool SgExecutor::ModeSense6(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return !sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

set<int> SgExecutor::ReportLuns(vector<uint8_t>&, span<uint8_t>)
{
    return {0};
}

int SgExecutor::ReadCapacity10(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, 8, 1).status;
}

int SgExecutor::ReadCapacity16(vector<uint8_t> &cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, 14, 1).status;
}

bool SgExecutor::ReadWrite(vector<uint8_t> &cdb, span<uint8_t> buf, int length)
{
    return !sg_adapter.SendCommand(cdb, buf, length, 10).status;
}

void SgExecutor::SynchronizeCache(vector<uint8_t> &cdb) const
{
    sg_adapter.SendCommand(cdb, { }, 0, 3);
}

int SgExecutor::Rewind(vector<uint8_t> &cdb) const
{
    return !sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status;
}

void SgExecutor::SpaceBack(vector<uint8_t> &cdb) const
{
    if (sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status) {
        throw io_exception("Can't space back one block");
    }
}

int SgExecutor::WriteFilemark(vector<uint8_t> &cdb) const
{
    return sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status;
}

int SgExecutor::Read(vector<uint8_t> &cdb, span<uint8_t> buf, int length, int timeout)
{
    return sg_adapter.SendCommand(cdb, buf, length, timeout).status;
}

int SgExecutor::Write(vector<uint8_t> &cdb, span<uint8_t> buf, int length, int timeout)
{
    if (sg_adapter.SendCommand(cdb, buf, length, timeout).status) {
        throw io_exception(fmt::format("Can't write block with {} byte(s)", length));
    }

    return length;
}
