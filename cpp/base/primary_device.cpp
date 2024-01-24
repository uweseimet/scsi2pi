//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "memory_util.h"
#include "primary_device.h"

using namespace std;
using namespace scsi_defs;
using namespace memory_util;

bool PrimaryDevice::Init(const param_map &params)
{
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

    SetParams(params);

    return true;
}

void PrimaryDevice::AddCommand(scsi_command cmd, const operation &execute)
{
    commands[cmd] = execute;
}

void PrimaryDevice::Dispatch(scsi_command cmd)
{
    if (const auto &it = commands.find(cmd); it != commands.end()) {
        LogDebug(fmt::format("Device is executing {0} (${1:02x})", COMMAND_MAPPING.find(cmd)->second.second,
            static_cast<int>(cmd)));

        it->second();
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
    return GetController() != nullptr ? GetController()->GetTargetId() : -1;
}

void PrimaryDevice::SetController(AbstractController *c)
{
    controller = c;

    device_logger.SetIdAndLun(GetId(), GetLun());
}

void PrimaryDevice::TestUnitReady()
{
    CheckReady();

    EnterStatusPhase();
}

void PrimaryDevice::Inquiry()
{
    // EVPD and page code check
    if ((GetController()->GetCmdByte(1) & 0x01) || GetController()->GetCmdByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const vector<uint8_t> buf = InquiryInternal();

    const size_t allocation_length = min(buf.size(), static_cast<size_t>(GetInt16(GetController()->GetCmd(), 3)));

    GetController()->CopyToBuffer(buf.data(), allocation_length);

    // Report if the device does not support the requested LUN
    if (const int lun = GetController()->GetEffectiveLun(); !GetController()->HasDeviceForLun(lun)) {
        LogTrace("LUN is not available");

        // Signal that the requested LUN does not exist
        GetController()->GetBuffer().data()[0] = 0x7f;
    }

    EnterDataInPhase();
}

void PrimaryDevice::ReportLuns()
{
    // Only SELECT REPORT mode 0 is supported
    if (GetController()->GetCmdByte(2)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const uint32_t allocation_length = GetInt32(GetController()->GetCmd(), 6);

    vector<uint8_t> &buf = GetController()->GetBuffer();
    fill_n(buf.begin(), min(buf.size(), static_cast<size_t>(allocation_length)), 0);

    uint32_t size = 0;
    for (int lun = 0; lun < GetController()->GetMaxLuns(); lun++) {
        if (GetController()->HasDeviceForLun(lun)) {
            size += 8;
            buf[size + 7] = (uint8_t)lun;
        }
    }

    SetInt16(buf, 2, size);

    size += 8;

    GetController()->SetLength(min(allocation_length, size));

    EnterDataInPhase();
}

void PrimaryDevice::RequestSense()
{
    int lun = GetController()->GetEffectiveLun();

    // Note: According to the SCSI specs the LUN handling for REQUEST SENSE non-existing LUNs do *not* result
    // in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
    if (!GetController()->HasDeviceForLun(lun)) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
        assert(GetController()->HasDeviceForLun(0));

        lun = 0;

        // Do not raise an exception here because the rest of the code must be executed
        GetController()->Error(sense_key::illegal_request, asc::invalid_lun);

        GetController()->SetStatus(status::good);
    }

    vector<byte> buf = GetController()->GetDeviceForLun(lun)->HandleRequestSense();

    const size_t allocation_length = min(buf.size(), static_cast<size_t>(GetController()->GetCmdByte(4)));

    GetController()->CopyToBuffer(buf.data(), allocation_length);

    // Clear the previous status
    SetStatusCode(0);

    EnterDataInPhase();
}

void PrimaryDevice::SendDiagnostic()
{
    // Do not support PF bit
    if (GetController()->GetCmdByte(1) & 0x10) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    // Do not support parameter list
    if ((GetController()->GetCmdByte(3) != 0) || (GetController()->GetCmdByte(4) != 0)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    EnterStatusPhase();
}

void PrimaryDevice::CheckReady()
{
    // Not ready if reset
    if (IsReset()) {
        SetReset(false);
        LogTrace("Device in reset");
        throw scsi_exception(sense_key::unit_attention, asc::power_on_or_reset);
    }

    // Not ready if it needs attention
    if (IsAttn()) {
        SetAttn(false);
        LogTrace("Device in needs attention");
        throw scsi_exception(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }

    // Return status if not ready
    if (!IsReady()) {
        LogTrace("Device not ready");
        throw scsi_exception(sense_key::not_ready, asc::medium_not_present);
    }

    LogTrace("Device is ready");
}

vector<uint8_t> PrimaryDevice::HandleInquiry(device_type type, scsi_level level, bool is_removable) const
{
    vector<uint8_t> buf(0x1F + 5);

    // Basic data
    // buf[0] ... SCSI device type
    // buf[1] ... Bit 7: Removable/not removable
    // buf[2] ... SCSI compliance level of command system
    // buf[3] ... SCSI compliance level of Inquiry response
    // buf[4] ... Inquiry additional data
    buf[0] = static_cast<uint8_t>(type);
    buf[1] = is_removable ? 0x80 : 0x00;
    buf[2] = static_cast<uint8_t>(level);
    buf[3] = level >= scsi_level::scsi_2 ?
                                           static_cast<uint8_t>(scsi_level::scsi_2) :
                                           static_cast<uint8_t>(scsi_level::scsi_1_ccs);
    buf[4] = 0x1F;

    // Padded vendor, product, revision
    memcpy(&buf.data()[8], GetPaddedName().c_str(), 28);

    return buf;
}

vector<byte> PrimaryDevice::HandleRequestSense() const
{
    // Return not ready only if there are no errors
    if (!GetStatusCode() && !IsReady()) {
        throw scsi_exception(sense_key::not_ready, asc::medium_not_present);
    }

    // Set 18 bytes including extended sense data

    vector<byte> buf(18);

    // Current error
    buf[0] = (byte)0x70;

    buf[2] = (byte)(GetStatusCode() >> 16);
    buf[7] = (byte)10;
    buf[12] = (byte)(GetStatusCode() >> 8);
    buf[13] = (byte)GetStatusCode();

    LogTrace(
        fmt::format("Status ${0:02x}, Sense Key ${1:02x}, ASC ${2:02x}", static_cast<int>(GetController()->GetStatus()),
            static_cast<int>(buf[2]), static_cast<int>(buf[12])));

    return buf;
}

bool PrimaryDevice::WriteByteSequence(span<const uint8_t>)
{
    LogError("Writing bytes is not supported by this device");

    return false;
}

void PrimaryDevice::ReserveUnit()
{
    reserving_initiator = GetController()->GetInitiatorId();

    if (reserving_initiator != -1) {
        LogTrace(fmt::format("Reserved device for initiator ID {}", reserving_initiator));
    }
    else {
        LogTrace("Reserved device for unknown initiator");
    }

    EnterStatusPhase();
}

void PrimaryDevice::ReleaseUnit()
{
    if (reserving_initiator != -1) {
        LogTrace(fmt::format("Released device reserved by initiator ID {}", reserving_initiator));
    }
    else {
        LogTrace("Released device reserved by unknown initiator");
    }

    DiscardReservation();

    EnterStatusPhase();
}

bool PrimaryDevice::CheckReservation(int initiator_id, scsi_command cmd, bool prevent_removal) const
{
    if (reserving_initiator == NOT_RESERVED || reserving_initiator == initiator_id) {
        return true;
    }

    // A reservation is valid for all commands except those excluded below
    if (cmd == scsi_command::cmd_inquiry || cmd == scsi_command::cmd_request_sense
        || cmd == scsi_command::cmd_release6) {
        return true;
    }
    // PREVENT ALLOW MEDIUM REMOVAL is permitted if the prevent bit is 0
    if (cmd == scsi_command::cmd_prevent_allow_medium_removal && !prevent_removal) {
        return true;
    }

    if (initiator_id != -1) {
        LogTrace(fmt::format("Initiator ID {} tries to access reserved device", initiator_id));
    }
    else {
        LogTrace("Unknown initiator tries to access reserved device");
    }

    return false;
}

void PrimaryDevice::DiscardReservation()
{
    reserving_initiator = NOT_RESERVED;
}
