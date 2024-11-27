//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Implementation of a generic SCSI device, using the Linux SG driver
//
//---------------------------------------------------------------------------

#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include "buses/bus_factory.h"
#include "shared/s2p_exceptions.h"
#include "scsi_generic.h"

using namespace s2p_util;

ScsiGeneric::ScsiGeneric(int lun) : PrimaryDevice(SCSG, scsi_level::scsi_2, lun)
{
    SetProduct("SCSI GENERIC");
    SupportsParams(true);
    SetReady(true);
}

bool ScsiGeneric::SetUp()
{
    device = GetParam(DEVICE);
    if (device.empty()) {
        LogError(fmt::format("Missing device file parameter"));
        return false;
    }

    if (!GetAsUnsignedInt(GetParam(TIMEOUT), timeout) || !timeout) {
        LogError(fmt::format("Invalid timeout value '{}'", GetParam(TIMEOUT)));
        return false;
    }

    fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        LogError(fmt::format("Can't open '{0}': {1}", device, strerror(errno)));
        return false;
    }

    return true;
}

void ScsiGeneric::CleanUp()
{
    if (fd != -1) {
        close(fd);
    }
}

void ScsiGeneric::Dispatch(scsi_command cmd)
{
    count = BusFactory::Instance().GetCommandBytesCount(cmd);
    if (!count) {
        LogTrace(fmt::format("Received unsupported command: ${:02x}", static_cast<int>(cmd)));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }

    const int allocation_length = BusFactory::Instance().GetAllocationLength(GetController()->GetCdb());

    GetController()->SetTransferSize(allocation_length, allocation_length);
    GetController()->SetCurrentLength(allocation_length);

    if (WRITE_COMMANDS.contains(static_cast<scsi_command>(GetController()->GetCdb()[0]))) {
        DataOutPhase(allocation_length);
    }
    else {
        DataInPhase(ReadData(GetController()->GetBuffer()));
    }
}

param_map ScsiGeneric::GetDefaultParams() const
{
    return {
        {   DEVICE, ""},
        {   TIMEOUT, "3"}
    };
}

vector<uint8_t> ScsiGeneric::InquiryInternal() const
{
    assert(false);
    return {};
}

int ScsiGeneric::ReadData(span<uint8_t> buf)
{
    return GetController()->GetRemainingLength() - ReadWriteData(buf.data(), false);
}

void ScsiGeneric::WriteData(span<const uint8_t> buf, scsi_command, int)
{
    ReadWriteData((void*)buf.data(), true);
}

int ScsiGeneric::ReadWriteData(void *buf, bool write) const
{
    assert(count);

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    if (GetController()->GetRemainingLength()) {
        io_hdr.dxfer_direction = write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }
    else {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }

    io_hdr.dxfer_len = GetController()->GetRemainingLength();
    io_hdr.dxferp = io_hdr.dxfer_len ? buf : nullptr;

    array<uint8_t, 13> sense_data = { };
    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = sense_data.size();

    vector<uint8_t> cdb;
    for (int i = 0; i < count; i++) {
        cdb.push_back(GetController()->GetCdb()[i]);
    }
    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = cdb.size();

    io_hdr.timeout = timeout * 1000;

    int status = ioctl(fd, SG_IO, &io_hdr) < 0 ? -1 : io_hdr.status;
    if (!status && sense_data[2] & 0x0f) {
        status = sense_data[2] & 0x0f;
    }

    if (status) {
        throw scsi_exception(static_cast<enum sense_key>(sense_data[2] & 0x0f), static_cast<enum asc>(sense_data[12]));
    }

    return io_hdr.resid;
}
