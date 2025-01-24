//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pdump_executor.h"
#include "shared/s2p_exceptions.h"
#include "shared/memory_util.h"
#include "shared/scsi.h"

using namespace memory_util;

void S2pDumpExecutor::TestUnitReady() const
{
    vector<uint8_t> cdb(6);

    TestUnitReady(cdb);
}

void S2pDumpExecutor::RequestSense(span<uint8_t> buf) const
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::REQUEST_SENSE);
    cdb[4] = static_cast<uint8_t>(buf.size());

    RequestSense(cdb, buf);
}

bool S2pDumpExecutor::Inquiry(span<uint8_t> buf) const
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::INQUIRY);
    cdb[4] = static_cast<uint8_t>(buf.size());

    return Inquiry(cdb, buf);
}

bool S2pDumpExecutor::ModeSense6(span<uint8_t> buf) const
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::MODE_SENSE_6);
    cdb[1] = 0x08;
    cdb[2] = 0x3f;
    cdb[4] = static_cast<uint8_t>(buf.size());

    return ModeSense6(cdb, buf);
}

set<int> S2pDumpExecutor::ReportLuns()
{
    vector<uint8_t> buf(512);
    vector<uint8_t> cdb(12);
    SetInt16(cdb, 8, buf.size());

    return ReportLuns(cdb, buf);
}

pair<uint64_t, uint32_t> S2pDumpExecutor::ReadCapacity() const
{
    vector<uint8_t> buf(14);
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CAPACITY_10);

    if (ReadCapacity10(cdb, buf)) {
        return {0, 0};
    }

    uint64_t capacity = GetInt32(buf, 0);

    int sector_size_offset = 4;

    if (static_cast<int32_t>(capacity) == -1) {
        cdb.resize(16);
        cdb[0] = static_cast<uint8_t>(ScsiCommand::READ_CAPACITY_READ_LONG_16);
        // READ CAPACITY(16), not READ LONG(16)
        cdb[1] = 0x10;

        if (ReadCapacity16(cdb, buf)) {
            return {0, 0};
        }

        capacity = GetInt64(buf, 0);

        sector_size_offset = 8;
    }

    return {capacity + 1, GetInt32(buf, sector_size_offset)};
}

bool S2pDumpExecutor::ReadWrite(span<uint8_t> buf, uint32_t bstart, uint32_t blength, int length, bool is_write)
{
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(is_write ? ScsiCommand::WRITE_10 : ScsiCommand::READ_10);
    SetInt32(cdb, 2, bstart);
    SetInt16(cdb, 7, blength);

    return ReadWrite(cdb, buf, length);
}

void S2pDumpExecutor::SynchronizeCache() const
{
    vector<uint8_t> cdb(10);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::SYNCHRONIZE_CACHE_10);

    SynchronizeCache(cdb);
}

void S2pDumpExecutor::SpaceBack() const
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::SPACE_6);
    cdb[1] = 0b000;
    SetInt24(cdb, 2, -1);

    SpaceBack(cdb);
}

int S2pDumpExecutor::Rewind()
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::REWIND);

    return Rewind(cdb);
}

int S2pDumpExecutor::WriteFilemark() const
{
    vector<uint8_t> cdb(6);
    cdb[0] = static_cast<uint8_t>(ScsiCommand::WRITE_FILEMARKS_6);
    SetInt24(cdb, 2, 1);

    return WriteFilemark(cdb);
}

int S2pDumpExecutor::ReadWrite(span<uint8_t> buf, int length)
{
    vector<uint8_t> cdb(6);

    // Restore
    if (length) {
        SetInt24(cdb, 2, length);

        if (Write(cdb, buf, length)) {
            throw IoException(fmt::format("Can't write block with {} byte(s)", length));
        }

        return length;
    }

    // Dump
    bool has_error = false;
    while (true) {
        SetInt24(cdb, 2, default_length);

        if (!Read(cdb, buf, default_length)) {
            GetLogger().debug("Read block with {} byte(s)", default_length);
            return default_length;
        }

        vector<uint8_t> sense_data(14);
        fill_n(cdb.begin(), cdb.size(), 0);
        cdb[4] = static_cast<uint8_t>(sense_data.size());
        const int status = RequestSense(cdb, sense_data);
        if (status == 0xff) {
            return status;
        }
        else if (status && status != 0x02) {
            throw IoException(fmt::format("Unknown error status {}", status));
        }

        const SenseKey sense_key = static_cast<SenseKey>(sense_data[2] & 0x0f);

        // EOD or EOM?
        if (sense_key == SenseKey::BLANK_CHECK || sense_data[2] & 0x40) {
            GetLogger().debug("No more data");
            return NO_MORE_DATA;
        }

        if (sense_key == SenseKey::MEDIUM_ERROR) {
            if (has_error) {
                return BAD_BLOCK;
            }

            has_error = true;

            SpaceBack();

            continue;
        }

        if (sense_data[2] & 0x80) {
            GetLogger().debug("Encountered filemark");
            return 0;
        }

        // VALID and ILI?
        if (sense_data[0] & 0x80 && sense_data[2] & 0x20) {
            length = default_length;

            default_length -= GetInt32(sense_data, 3);

            // If all available data have been read there is no need to re-try
            if (default_length < length) {
                GetLogger().debug("Read block with {} byte(s)", default_length);

                return default_length;
            }

            SpaceBack();
        }
        else {
            return 0xff;
        }
    }
}

void S2pDumpExecutor::SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    buf[offset] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> 16);
    buf[offset + 1] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}
