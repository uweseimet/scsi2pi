//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_executor.h"
#include "shared/memory_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/sg_adapter.h"

using namespace memory_util;

void SgExecutor::TestUnitReady(span<uint8_t> cdb) const
{
    sg_adapter.SendCommand(cdb, { }, 0, 1);
}

int SgExecutor::RequestSense(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

bool SgExecutor::Inquiry(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return !sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

bool SgExecutor::ModeSense6(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return !sg_adapter.SendCommand(cdb, buf, static_cast<int>(buf.size()), 1).status;
}

set<int> SgExecutor::ReportLuns(span<uint8_t>, span<uint8_t>)
{
    return {0};
}

int SgExecutor::ReadCapacity10(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, 8, 1).status;
}

int SgExecutor::ReadCapacity16(span<uint8_t> cdb, span<uint8_t> buf) const
{
    return sg_adapter.SendCommand(cdb, buf, 14, 1).status;
}

bool SgExecutor::ReadWrite(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return !sg_adapter.SendCommand(cdb, buf, length, LONG_TIMEOUT).status;
}

void SgExecutor::SynchronizeCache(span<uint8_t> cdb) const
{
    sg_adapter.SendCommand(cdb, { }, 0, 3);
}

int SgExecutor::Rewind(span<uint8_t> cdb) const
{
    return !sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status;
}

void SgExecutor::SpaceBack(span<uint8_t> cdb) const
{
    if (sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status) {
        throw IoException("Can't space back one block");
    }
}

int SgExecutor::WriteFilemark(span<uint8_t> cdb) const
{
    return sg_adapter.SendCommand(cdb, { }, 0, LONG_TIMEOUT).status;
}

bool SgExecutor::Read(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return sg_adapter.SendCommand(cdb, buf, length, LONG_TIMEOUT).status;
}

bool SgExecutor::Write(span<uint8_t> cdb, span<uint8_t> buf, int length)
{
    return sg_adapter.SendCommand(cdb, buf, length, LONG_TIMEOUT).status;
}
