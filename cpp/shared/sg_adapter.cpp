//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sg_adapter.h"
#include <array>
#include <iostream>
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <spdlog/spdlog.h>
#include "shared/command_meta_data.h"

using namespace spdlog;

string SgAdapter::Init(const string &device)
{
    if (!device.starts_with("/dev/sg")) {
        return fmt::format("Missing or invalid device file: '{}'", device);
    }

    fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        return fmt::format("Can't open '{0}': {1}", device, strerror(errno));
    }

    if (int v; ioctl(fd, SG_GET_VERSION_NUM, &v) < 0 || v < 30000) {
        CleanUp();
        return fmt::format("{0} does not appear to be a Linux SG 3 driver device: {1}", device, strerror(errno));
    }

    return "";
}

void SgAdapter::CleanUp()
{
    if (fd != -1) {
        close (fd);
        fd = -1;
    }
}

SgAdapter::SgResult SgAdapter::SendCommand(span<uint8_t> cdb, span<uint8_t> buf, int length, int timeout) const
{
    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (buf.empty()) {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
    else {
        io_hdr.dxfer_direction =
            CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(cdb[0])).has_data_out ?
                SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf.data() : nullptr;

    array<uint8_t, 18> sense_data = { };
    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = sense_data.size();

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    const int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;

    return {status, length - io_hdr.resid, static_cast<sense_key>(static_cast<int>(sense_data[2]) & 0x0f)};
}
