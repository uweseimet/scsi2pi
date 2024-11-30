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
    // There is no explicit LUN support, the SG driver maps each LUN to a device file
    if (GetController()->GetCdb()[1] & 0b11100000) {
        if (cmd == scsi_command::inquiry) {
            inquiry_for_lun_x = true;
        }
        else {
            auto &buf = GetController()->GetBuffer();

            fill_n(buf.begin(), 18, 0);
            buf[0] = 0x70;
            buf[2] = static_cast<uint8_t>(sense_key::illegal_request);
            buf[7] = 10;
            buf[12] = static_cast<uint8_t>(asc::invalid_lun);

            GetController()->DataIn();

            return;
        }
    }

    if (cmd == scsi_command::request_sense && deferred_sense_key != sense_key::no_sense) {
        GetController()->SetCurrentLength(18);

        auto &buf = GetController()->GetBuffer();

        fill_n(buf.begin(), 18, 0);
        buf[0] = 0x70;
        buf[2] = static_cast<uint8_t>(deferred_sense_key);
        buf[7] = 10;
        buf[12] = static_cast<uint8_t>(deferred_asc);
        buf[13] = deferred_ascq;

        deferred_sense_key = sense_key::no_sense;
        deferred_asc = asc::no_additional_sense_information;
        deferred_ascq = 0;

        GetController()->DataIn();

        return;
    }

    count = BusFactory::Instance().GetCommandBytesCount(cmd);
    assert(count);

    const int allocation_length = BusFactory::Instance().GetAllocationLength(GetController()->GetCdb());

    GetController()->SetTransferSize(allocation_length, allocation_length);
    GetController()->SetCurrentLength(allocation_length);

    if (WRITE_COMMANDS.contains(cmd)) {
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

int ScsiGeneric::ReadData(data_in_t buf)
{
    return GetController()->GetRemainingLength() - ReadWriteData(buf.data(), false);
}

void ScsiGeneric::WriteData(data_out_t buf, scsi_command, int)
{
    ReadWriteData((void*)buf.data(), true);
}

int ScsiGeneric::ReadWriteData(void *buf, bool write) // NOSONAR SG driver API requires void *
{
    assert(count);

    sg_io_hdr io_hdr = { };

    io_hdr.interface_id = 'S';

    const int length = BusFactory::Instance().GetAllocationLength(GetController()->GetCdb());

    if (length) {
        io_hdr.dxfer_direction = write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
    }
    else {
        io_hdr.dxfer_direction = SG_DXFER_NONE;
    }

    io_hdr.dxfer_len = length;
    io_hdr.dxferp = io_hdr.dxfer_len ? buf : nullptr;

    array<uint8_t, 13> sense_data = { };
    io_hdr.sbp = sense_data.data();
    io_hdr.mx_sb_len = sense_data.size();

    vector<uint8_t> cdb;
    for (int i = 0; i < count; i++) {
        cdb.push_back(static_cast<uint8_t>(GetController()->GetCdb()[i]));
    }
    // Enforce LUN 0
    cdb[1] &= 0b00011111;

    io_hdr.cmdp = cdb.data();
    io_hdr.cmd_len = static_cast<uint8_t>(cdb.size());

    io_hdr.timeout = timeout * 1000;

    LogTrace(fmt::format("Executing command ${0:02x} with SG driver, transfer length is {1} byte(s)", cdb[0], length));

    if (ioctl(fd, SG_IO, &io_hdr) == -1) {
        LogError(fmt::format("SCSI transfer of {0} byte(s) failed: {1}", length, strerror(errno)));
        throw scsi_exception(sense_key::aborted_command, write ? asc::write_error : asc::read_error);
    }

    int status = io_hdr.status;
    if (!status && static_cast<int>(sense_data[2]) & 0x0f) {
        status = static_cast<int>(sense_data[2]) & 0x0f;
    }

    if (status) {
        deferred_sense_key = static_cast<enum sense_key>(static_cast<int>(sense_data[2]) & 0x0f);
        deferred_asc = static_cast<enum asc>(sense_data[12]);
        deferred_ascq = sense_data[13];
        throw scsi_exception(deferred_sense_key, deferred_asc);
    }
    else if (inquiry_for_lun_x) {
        // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
        GetController()->GetBuffer().data()[0] = 0x7f;
    }

    return io_hdr.resid;
}
