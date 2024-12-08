//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "primary_device.h"
#include "shared/command_meta_data.h"

using namespace memory_util;
using namespace s2p_util;

bool PrimaryDevice::Init()
{
    // Mandatory SCSI primary commands
    AddCommand(scsi_command::test_unit_ready, [this]
        {
            TestUnitReady();
        });
    AddCommand(scsi_command::inquiry, [this]
        {
            Inquiry();
        });
    AddCommand(scsi_command::report_luns, [this]
        {
            ReportLuns();
        });

    // Optional commands supported by all device types
    AddCommand(scsi_command::request_sense, [this]
        {
            RequestSense();
        });
    AddCommand(scsi_command::reserve_6, [this]
        {
            ReserveUnit();
        });
    AddCommand(scsi_command::release_6, [this]
        {
            ReleaseUnit();
        });
    AddCommand(scsi_command::send_diagnostic, [this]
        {
            SendDiagnostic();
        });

    return SetUp();
}

void PrimaryDevice::AddCommand(scsi_command cmd, const command &c)
{
    assert(!commands[static_cast<int>(cmd)]);
    commands[static_cast<int>(cmd)] = c;
}

void PrimaryDevice::Dispatch(scsi_command cmd)
{
    if (const auto &command = commands[static_cast<int>(cmd)]; command) {
        LogDebug(fmt::format("Device is executing {0} (${1:02x})", CommandMetaData::Instance().GetCommandName(cmd),
                static_cast<int>(cmd)));
        command();
    }
    else {
        LogTrace(fmt::format("Device received unsupported command: ${:02x}", static_cast<int>(cmd)));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
    }
}

void PrimaryDevice::Reset()
{
    DiscardReservation();

    SetReset(false);
    SetAttn(false);
    SetLocked(false);
}

void PrimaryDevice::SetStatus(enum sense_key s, enum asc a)
{
    sense_key = s;
    asc = a;
}

void PrimaryDevice::ResetStatus()
{
    sense_key = sense_key::no_sense;
    asc = asc::no_additional_sense_information;
    valid = false;
    filemark = false;
    ili = false;
    information = 0;
    eom = ascq::none;
}

void PrimaryDevice::SetFilemark()
{
    filemark = true;
}

void PrimaryDevice::SetEom(ascq e)
{
    eom = e;
}

void PrimaryDevice::SetIli()
{
    ili = true;
}

void PrimaryDevice::SetInformation(int64_t value)
{
    information = static_cast<int32_t>(value);
    valid = true;
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
    // EVPD, CMDDT and page code check
    if ((GetCdbByte(1) & 0x03) || GetCdbByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const auto &buf = InquiryInternal();

    const int allocation_length = min(static_cast<int>(buf.size()), GetCdbInt16(3));

    GetController()->CopyToBuffer(buf.data(), allocation_length);

    // Report if the device does not support the requested LUN
    if (!GetController()->GetDeviceForLun(GetController()->GetEffectiveLun())) {
        // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
        GetController()->GetBuffer().data()[0] = 0x7f;
    }

    DataInPhase(allocation_length);
}

void PrimaryDevice::ReportLuns()
{
    // Only SELECT REPORT mode 0 is supported
    if (GetCdbByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint32_t allocation_length = GetCdbInt32(6);

    auto &buf = GetController()->GetBuffer();
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
    // The descriptor format is not supported
    if (GetController()->GetCdb()[1] & 0x01) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    int effective_lun = GetController()->GetEffectiveLun();

    // According to the specification the LUN handling for REQUEST SENSE for non-existing LUNs does not result
    // in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
    if (!GetController()->GetDeviceForLun(effective_lun)) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
        assert(GetController()->GetDeviceForLun(0));

        effective_lun = 0;

        // When signalling an invalid LUN the status must be GOOD
        GetController()->Error(sense_key::illegal_request, asc::logical_unit_not_supported, status_code::good);
    }

    const vector<byte> &buf = GetController()->GetDeviceForLun(effective_lun)->HandleRequestSense();

    int allocation_length = GetCdbByte(4);
    if (!allocation_length && level == scsi_level::scsi_1_ccs) {
        allocation_length = 4;
    }

    const int length = min(static_cast<int>(buf.size()), allocation_length);
    GetController()->CopyToBuffer(buf.data(), length);

    ResetStatus();

    DataInPhase(length);
}

void PrimaryDevice::SendDiagnostic()
{
    // Do not support parameter list
    if (GetCdbByte(3) || GetCdbByte(4)) {
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
    // Signal support of linked commands
    buf[7] = 0x08;
    // TODO Temporary: Signal support of linked commands and synchronous transfers
    buf[7] = 0x18;

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

    vector<byte> buf(18);

    // In SCSI-1 mode only return the extended format if more than 4 bytes have been requested
    const bool extended = level >= scsi_level::scsi_2 || GetCdbByte(4) > 4;

    if (extended) {
        // Current error
        buf[0] = byte { 0x70 };
    }

    if (valid) {
        buf[0] |= byte { 0x80 };
        SetInt32(buf, extended ? 3 : 1, information);
    }

    buf[2] = static_cast<byte>(sense_key) | (ili ? byte { 0x20 } : byte { 0x00 });
    buf[7] = byte { 10 };
    buf[12] = static_cast<byte>(asc);

    if (filemark) {
        buf[2] |= byte { 0x80 };
        buf[13] = static_cast<byte>(ascq::filemark_detected);
    }

    if (eom != ascq::none) {
        buf[2] |= byte { 0x40 };
        buf[13] = static_cast<byte>(eom);
    }

    LogTrace(fmt::format("{0}: {1}", STATUS_MAPPING.at(GetController()->GetStatus()), FormatSenseData(buf)));

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
    const auto cmd = static_cast<scsi_command>(GetCdbByte(0));
    if (cmd == scsi_command::inquiry || cmd == scsi_command::request_sense
        || cmd == scsi_command::release_6) {
        return true;
    }

    // PREVENT ALLOW MEDIUM REMOVAL is permitted if the prevent bit is 0
    if (cmd == scsi_command::prevent_allow_medium_removal && !(GetCdbByte(4) & 0x01)) {
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
