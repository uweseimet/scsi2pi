//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "primary_device.h"
#include "buses/bus_factory.h"
#include "memory_util.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;
using namespace s2p_util;

bool PrimaryDevice::Init(const param_map &params)
{
    SetParams(params);

    // Mandatory SCSI primary commands
    AddCommand(scsi_command::cmd_test_unit_ready, [this]
        {
            TestUnitReady();
        });
    AddCommand(scsi_command::cmd_inquiry, [this]
        {
            Inquiry();
        });
    AddCommand(scsi_command::cmd_report_luns, [this]
        {
            ReportLuns();
        });

    // Optional commands supported by all device types
    AddCommand(scsi_command::cmd_request_sense, [this]
        {
            RequestSense();
        });
    AddCommand(scsi_command::cmd_reserve6, [this]
        {
            ReserveUnit();
        });
    AddCommand(scsi_command::cmd_release6, [this]
        {
            ReleaseUnit();
        });
    AddCommand(scsi_command::cmd_send_diagnostic, [this]
        {
            SendDiagnostic();
        });

    return SetUp();
}

void PrimaryDevice::AddCommand(scsi_command cmd, const command &c)
{
    commands[static_cast<int>(cmd)] = c;
}

void PrimaryDevice::Dispatch(scsi_command cmd)
{
    if (const auto &command = commands[static_cast<int>(cmd)]; command) {
        LogDebug(fmt::format("Device is executing {0} (${1:02x})", BusFactory::Instance().GetCommandName(cmd),
                static_cast<int>(cmd)));
        command();
    }
    else {
        LogTrace(fmt::format("Received unsupported command: ${:02x}", static_cast<int>(cmd)));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }
}

void PrimaryDevice::Reset()
{
    DiscardReservation();

    Device::Reset();
}

int PrimaryDevice::GetId() const
{
    return GetController() ? GetController()->GetTargetId() : -1;
}

bool PrimaryDevice::SetScsiLevel(scsi_level l)
{
    if (l == scsi_level::none || l >= scsi_level::last) {
        return false;
    }

    level = l;

    return true;
}

void PrimaryDevice::SetController(AbstractController *c)
{
    controller = c;

    device_logger.SetIdAndLun(GetId(), GetLun());
}

void PrimaryDevice::StatusPhase() const
{
    controller->Status();
}

void PrimaryDevice::DataInPhase(int length) const
{
    controller->SetCurrentLength(length);
    controller->DataIn();
}

void PrimaryDevice::DataOutPhase(int length) const
{
    controller->SetCurrentLength(length);
    controller->DataOut();
}

void PrimaryDevice::TestUnitReady()
{
    CheckReady();

    StatusPhase();
}

void PrimaryDevice::Inquiry()
{
    // EVPD and page code check
    if ((GetController()->GetCdbByte(1) & 0x01) || GetController()->GetCdbByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const vector<uint8_t> &buf = InquiryInternal();

    const size_t allocation_length = min(buf.size(), static_cast<size_t>(GetInt16(GetController()->GetCdb(), 3)));

    GetController()->CopyToBuffer(buf.data(), allocation_length);

    // Report if the device does not support the requested LUN
    if (!GetController()->GetDeviceForLun(GetController()->GetEffectiveLun())) {
        // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
        GetController()->GetBuffer().data()[0] = 0x7f;
    }

    DataInPhase(static_cast<int>(allocation_length));
}

void PrimaryDevice::ReportLuns()
{
    // Only SELECT REPORT mode 0 is supported
    if (GetController()->GetCdbByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint32_t allocation_length = GetInt32(GetController()->GetCdb(), 6);

    vector<uint8_t> &buf = GetController()->GetBuffer();
    fill_n(buf.begin(), min(buf.size(), static_cast<size_t>(allocation_length)), 0);

    uint32_t size = 0;
    for (int lun = 0; lun < 32; lun++) {
        if (GetController()->GetDeviceForLun(lun)) {
            size += 8;
            buf[size + 7] = static_cast<uint8_t>(lun);
        }
    }

    SetInt16(buf, 2, size);

    DataInPhase(min(allocation_length, size + 8));
}

void PrimaryDevice::RequestSense()
{
    int effective_lun = GetController()->GetEffectiveLun();

    // According to the specification the LUN handling for REQUEST SENSE for non-existing LUNs does not result
    // in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
    if (!GetController()->GetDeviceForLun(effective_lun)) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
        assert(GetController()->GetDeviceForLun(0));

        effective_lun = 0;

        // When signalling an invalid LUN the status must be GOOD
        GetController()->Error(sense_key::illegal_request, asc::invalid_lun, status_code::good);
    }

    const vector<byte> &buf = GetController()->GetDeviceForLun(effective_lun)->HandleRequestSense();

    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetController()->GetCdbByte(4))));
    GetController()->CopyToBuffer(buf.data(), length);

    // Clear the previous status
    SetStatus(sense_key::no_sense, asc::no_additional_sense_information);
    valid = false;
    filemark = false;
    eom = false;

    DataInPhase(length);
}

void PrimaryDevice::SendDiagnostic()
{
    // Do not support parameter list
    if (GetController()->GetCdbByte(3) || GetController()->GetCdbByte(4)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    StatusPhase();
}

void PrimaryDevice::CheckReady()
{
    // Not ready if reset
    if (IsReset()) {
        SetReset(false);
        throw scsi_exception(sense_key::unit_attention, asc::power_on_or_reset);
    }

    // Not ready if it needs attention
    if (IsAttn()) {
        SetAttn(false);
        throw scsi_exception(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }

    // Return status if not ready
    if (!IsReady()) {
        throw scsi_exception(sense_key::not_ready, asc::medium_not_present);
    }
}

vector<uint8_t> PrimaryDevice::HandleInquiry(device_type type, bool is_removable) const
{
    vector<uint8_t> buf(0x1f + 5);

    buf[0] = static_cast<uint8_t>(type);
    buf[1] = is_removable ? 0x80 : 0x00;
    buf[2] = static_cast<uint8_t>(level);
    buf[3] = level >= scsi_level::scsi_2 ?
            static_cast<uint8_t>(scsi_level::scsi_2) : static_cast<uint8_t>(scsi_level::scsi_1_ccs);
    buf[4] = 0x1f;

    // Padded vendor, product, revision
    memcpy(&buf.data()[8], GetPaddedName().c_str(), 28);

    return buf;
}

vector<byte> PrimaryDevice::HandleRequestSense() const
{
    // Return not ready only if there are no errors
    if (sense_key == sense_key::no_sense && !IsReady()) {
        throw scsi_exception(sense_key::not_ready, asc::medium_not_present);
    }

    // 18 bytes including extended sense data
    vector<byte> buf(18);

    // Current error
    buf[0] = (byte)0x70;

    buf[2] = (byte)sense_key | (filemark ? (byte)0x80 : (byte)0x00) | (eom ? (byte)0x40 : (byte)0x00);
    buf[7] = (byte)10;
    buf[12] = (byte)asc;

    if (valid) {
        buf[0] |= (byte)0x80;
        SetInt32(buf, 3, information);
    }

    LogTrace(fmt::format("{0}: {1}", STATUS_MAPPING.at(GetController()->GetStatus()), FormatSenseData(sense_key, asc)));

    return buf;
}

void PrimaryDevice::ReserveUnit()
{
    reserving_initiator = GetController()->GetInitiatorId();

    StatusPhase();
}

void PrimaryDevice::ReleaseUnit()
{
    DiscardReservation();

    StatusPhase();
}

bool PrimaryDevice::CheckReservation(int initiator_id) const
{
    if (reserving_initiator == NOT_RESERVED || reserving_initiator == initiator_id) {
        return true;
    }

    // A reservation is valid for all commands except those excluded below
    const auto cmd = static_cast<scsi_command>(GetController()->GetCdbByte(0));
    if (cmd == scsi_command::cmd_inquiry || cmd == scsi_command::cmd_request_sense
        || cmd == scsi_command::cmd_release6) {
        return true;
    }

    // PREVENT ALLOW MEDIUM REMOVAL is permitted if the prevent bit is 0
    if (cmd == scsi_command::cmd_prevent_allow_medium_removal && !(GetController()->GetCdbByte(4) & 0x01)) {
        return true;
    }

    if (initiator_id != -1) {
        LogTrace(fmt::format("Initiator ID {} tries to access reserved device", initiator_id));
    }
    else {
        LogTrace("Unknown initiator tries to access reserved device");
    }

    GetController()->Error(sense_key::aborted_command, asc::no_additional_sense_information,
        status_code::reservation_conflict);

    return false;
}

void PrimaryDevice::DiscardReservation()
{
    reserving_initiator = NOT_RESERVED;
}
