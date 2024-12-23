//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_util.h"
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <spdlog/spdlog.h>
#include "command_meta_data.h"
#include "memory_util.h"
#include "s2p_exceptions.h"

using namespace spdlog;
using namespace memory_util;

int sg_util::OpenDevice(const string &device)
{
    if (!device.starts_with("/dev/sg")) {
        throw io_exception(fmt::format("Missing or invalid device file: '{}'", device));
    }

    const int fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        throw io_exception(fmt::format("Can't open '{0}': {1}", device, strerror(errno)));
    }

    if (int v; ioctl(fd, SG_GET_VERSION_NUM, &v) < 0 || v < 30000) {
        close (fd);
        throw io_exception(
            fmt::format("'{0}' is not supported by the Linux SG 3 driver: {1}", device, strerror(errno)));
    }

    return fd;
}

int sg_util::GetAllocationLength(span<uint8_t> cdb)
{
    const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0]));

    // For commands without allocation length field the length is coded as a negative offset
    if (meta_data.allocation_length_offset < 0) {
        return -meta_data.allocation_length_offset;
    }

    switch (meta_data.allocation_length_size) {
    case 0:
        break;

    case 1:
        return cdb[meta_data.allocation_length_offset];

    case 2:
        return GetInt16(cdb, meta_data.allocation_length_offset);

    case 3:
        return GetInt24(cdb, meta_data.allocation_length_offset);

    case 4:
        return GetInt32(cdb, meta_data.allocation_length_offset);

    default:
        assert(false);
        break;
    }

    return 0;
}

void sg_util::UpdateStartBlock(span<uint8_t> cdb, int length)
{
    switch (const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])); meta_data.block_size) {
    case 3:
        SetInt24(cdb, meta_data.block_offset, GetInt24(cdb, meta_data.block_offset) + length);
        break;

    case 4:
        SetInt32(cdb, meta_data.block_offset, GetInt32(cdb, meta_data.block_offset) + length);
        break;

    case 8:
        SetInt64(cdb, meta_data.block_offset, GetInt64(cdb, meta_data.block_offset) + length);
        break;

    default:
        break;
    }
}

void sg_util::SetBlockCount(span<uint8_t> cdb, int length)
{
    const auto &meta_data = CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0]));
    if (meta_data.block_size) {
        switch (meta_data.allocation_length_size) {
        case 1:
            cdb[meta_data.allocation_length_offset] = static_cast<uint8_t>(length);
            break;

        case 2:
            SetInt16(cdb, meta_data.allocation_length_offset, length);
            break;

        case 4:
            SetInt32(cdb, meta_data.allocation_length_offset, length);
            break;

        default:
            assert(false);
            break;
        }
    }
}

void sg_util::SetInt24(span<uint8_t> buf, int offset, int value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    buf[offset] = static_cast<uint8_t>(value >> 16);
    buf[offset + 1] = static_cast<uint8_t>(value >> 8);
    buf[offset + 2] = static_cast<uint8_t>(value);
}
