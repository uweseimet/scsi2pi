//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "primary_device.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include "controllers/abstract_controller.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;
using namespace s2p_util;

string PrimaryDevice::Init()
{
    // Mandatory SCSI primary commands
    AddCommand(ScsiCommand::TEST_UNIT_READY, [this]
        {
            CheckReady();
            StatusPhase();
        });
    AddCommand(ScsiCommand::INQUIRY, [this]
        {
            Inquiry();
        });
    AddCommand(ScsiCommand::REPORT_LUNS, [this]
        {
            ReportLuns();
        });

    // Optional commands supported by all device types
    AddCommand(ScsiCommand::REQUEST_SENSE, [this]
        {
            RequestSense();
        });
    AddCommand(ScsiCommand::RESERVE_RESERVE_ELEMENT_6, [this]
        {
            reserving_initiator = controller->GetInitiatorId();
            StatusPhase();
        });
    AddCommand(ScsiCommand::RELEASE_RELEASE_ELEMENT_6, [this]
        {
            DiscardReservation();
            StatusPhase();
        });
    AddCommand(ScsiCommand::SEND_DIAGNOSTIC, [this]
        {
            SendDiagnostic();
        });

    return SetUp();
}

void PrimaryDevice::AddCommand(ScsiCommand cmd, const command &c)
{
    assert(!commands[static_cast<int>(cmd)]);
    commands[static_cast<int>(cmd)] = c;
}

void PrimaryDevice::Dispatch(ScsiCommand cmd)
{
    if (const auto &command = commands[static_cast<int>(cmd)]; command) {
        LogDebug(fmt::format("Device is executing {0} (${1:02x})", CommandMetaData::GetInstance().GetCommandName(cmd),
                static_cast<int>(cmd)));
        command();
    }
    else {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
    }
}

void PrimaryDevice::Reset()
{
    DiscardReservation();

    SetReset(false);
    SetAttn(false);
    SetLocked(false);
}

void PrimaryDevice::SetStatus(enum SenseKey s, enum Asc a)
{
    sense_key = s;
    asc = a;
}

void PrimaryDevice::ResetStatus()
{
    sense_key = SenseKey::NO_SENSE;
    asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION;
    valid = false;
    filemark = false;
    ili = false;
    information = 0;
    eom = Ascq::NONE;
}

void PrimaryDevice::SetFilemark()
{
    filemark = true;
}

void PrimaryDevice::SetEom(Ascq e)
{
    eom = e;
}

void PrimaryDevice::SetIli()
{
    ili = true;
}

void PrimaryDevice::SetInformation(int32_t value)
{
    information = value;
    valid = true;
}

int PrimaryDevice::GetId() const
{
    return controller ? controller->GetTargetId() : -1;
}

string PrimaryDevice::SetProductData(const ProductData &data, bool force)
{
    if (string_view vendor = Trim(data.vendor); !vendor.empty()) {
        if (vendor.length() > 8) {
            return "Vendor '" + data.vendor + "' must have between 1 and 8 characters";
        }

        product_data.vendor = string(vendor);
    }

    if (string_view product = Trim(data.product); !product.empty()) {
        if (product.length() > 16) {
            return "Product '" + data.product + "' must have between 1 and 16 characters";
        }

        // Changing existing vital product data is not SCSI compliant
        if (product_data.product.empty() || force) {
            product_data.product = string(product);
        }
    }

    if (string_view revision = Trim(data.revision); !revision.empty()) {
        if (revision.length() > 4) {
            return "Revision '" + data.revision + "' must have between 1 and 4 characters";
        }

        product_data.revision = string(revision);
    }

    return "";
}

PrimaryDevice::ProductData PrimaryDevice::GetProductData() const
{
    return product_data;
}

bool PrimaryDevice::SetScsiLevel(ScsiLevel l)
{
    if (l >= ScsiLevel::LAST) {
        return false;
    }

    level = l;

    return true;
}

bool PrimaryDevice::SetResponseDataFormat(ScsiLevel l)
{
    if (l == ScsiLevel::NONE || l > ScsiLevel::SCSI_2) {
        return false;
    }

    response_data_format = l;

    return true;
}

void PrimaryDevice::SetController(AbstractController *c)
{
    controller = c;

    CreateLogger();
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

void PrimaryDevice::Inquiry()
{
    // Reserved bits, EVPD, CMDDT and page code check
    if ((GetCdbByte(1) & 0x1f) || GetCdbByte(2)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const auto &buf = InquiryInternal();

    const int allocation_length = min(static_cast<int>(buf.size()), GetCdbInt16(3));

    controller->CopyToBuffer(buf.data(), allocation_length);

    // Report if the device does not support the requested LUN
    if (!controller->GetDeviceForLun(controller->GetEffectiveLun())) {
        // SCSI-2 section 8.2.5.1: Incorrect logical unit handling
        controller->GetBuffer().data()[0] = 0x7f;
    }

    DataInPhase(allocation_length);
}

void PrimaryDevice::ReportLuns() const
{
    // Only SELECT REPORT mode 0 is supported
    if (GetCdbByte(2)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const uint32_t allocation_length = GetCdbInt32(6);

    auto &buf = controller->GetBuffer();
    fill_n(buf.begin(), min(buf.size(), static_cast<size_t>(allocation_length)), 0);

    uint32_t size = 0;
    for (int l = 0; l < 32; ++l) {
        if (controller->GetDeviceForLun(l)) {
            size += 8;
            buf[size + 7] = static_cast<uint8_t>(l);
        }
    }

    SetInt16(buf, 2, size);

    DataInPhase(min(allocation_length, size + 8));
}

void PrimaryDevice::RequestSense()
{
    // The descriptor format is not supported
    if (GetCdbByte(1) & 0x01) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    int effective_lun = controller->GetEffectiveLun();

    // According to the specification REQUEST SENSE for non-existing LUNs does not report CHECK CONDITION.
    // Only the Sense Key and ASC are set in order to signal the non-existing LUN.
    if (!controller->GetDeviceForLun(effective_lun)) {
        assert(controller->GetDeviceForLun(0));

        effective_lun = 0;

        // When signalling an invalid LUN the status must be GOOD
        controller->Error(SenseKey::ILLEGAL_REQUEST, Asc::LOGICAL_UNIT_NOT_SUPPORTED, StatusCode::GOOD);
    }

    const vector<byte> &buf = controller->GetDeviceForLun(effective_lun)->HandleRequestSense();

    int allocation_length = GetCdbByte(4);
    if (!allocation_length && level == ScsiLevel::SCSI_1_CCS) {
        allocation_length = 4;
    }

    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(allocation_length)));
    controller->CopyToBuffer(buf.data(), length);

    ResetStatus();

    DataInPhase(length);
}

void PrimaryDevice::SendDiagnostic() const
{
    // Do not support parameter list
    if (GetCdbByte(3) || GetCdbByte(4)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    StatusPhase();
}

void PrimaryDevice::CheckReady()
{
    // Not ready if reset
    if (IsReset()) {
        SetReset(false);
        throw ScsiException(SenseKey::UNIT_ATTENTION, Asc::POWER_ON_OR_RESET);
    }

    // Not ready if it needs attention
    if (IsAttn()) {
        SetAttn(false);
        throw ScsiException(SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_TRANSITION);
    }

    // Return status if not ready
    if (!IsReady()) {
        throw ScsiException(SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);
    }
}

vector<uint8_t> PrimaryDevice::HandleInquiry(DeviceType device_type, bool is_removable) const
{
    vector<uint8_t> buf(0x1f + 5);

    buf[0] = static_cast<uint8_t>(device_type);
    buf[1] = is_removable ? 0x80 : 0x00;
    buf[2] = static_cast<uint8_t>(level);
    buf[3] = level >= ScsiLevel::SCSI_2 ?
            static_cast<uint8_t>(ScsiLevel::SCSI_2) : static_cast<uint8_t>(ScsiLevel::SCSI_1_CCS);
    buf[4] = 0x1f;
    // Signal support of linked commands
    buf[7] = 0x08;

    memcpy(buf.data() + 8, GetPaddedName().c_str(), 28);

    return buf;
}

vector<byte> PrimaryDevice::HandleRequestSense() const
{
    // Return not ready only if there are no errors
    if (sense_key == SenseKey::NO_SENSE && !IsReady()) {
        throw ScsiException(SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);
    }

    vector<byte> buf(18);

    // In SCSI-1 mode only return the extended format if more than 4 bytes have been requested
    const bool extended = level >= ScsiLevel::SCSI_2 || GetCdbByte(4) > 4;

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
        buf[13] = static_cast<byte>(Ascq::FILEMARK_DETECTED);
    }

    if (eom != Ascq::NONE) {
        buf[2] |= byte { 0x40 };
        buf[13] = static_cast<byte>(eom);
    }

    LogTrace(fmt::format("Status {0}: {1}", STATUS_MAPPING.at(controller->GetStatus()), FormatSenseData(buf)));

    return buf;
}

bool PrimaryDevice::CheckReservation(int initiator_id) const
{
    if (reserving_initiator == NOT_RESERVED || reserving_initiator == initiator_id) {
        return true;
    }

    // A reservation is valid for all commands except those excluded below
    const auto cmd = static_cast<ScsiCommand>(GetCdbByte(0));
    if (cmd == ScsiCommand::INQUIRY || cmd == ScsiCommand::REQUEST_SENSE
        || cmd == ScsiCommand::RELEASE_RELEASE_ELEMENT_6) {
        return true;
    }

    // PREVENT ALLOW MEDIUM REMOVAL is permitted if the prevent bit is 0
    if (cmd == ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL && !(GetCdbByte(4) & 0x01)) {
        return true;
    }

    if (initiator_id != -1) {
        LogTrace(fmt::format("Initiator ID {} tries to access reserved device", initiator_id));
    }
    else {
        LogTrace("Unknown initiator tries to access reserved device");
    }

    controller->Error(SenseKey::ILLEGAL_REQUEST, Asc::NO_ADDITIONAL_SENSE_INFORMATION,
        StatusCode::RESERVATION_CONFLICT);

    return false;
}

void PrimaryDevice::DiscardReservation()
{
    reserving_initiator = NOT_RESERVED;
}

void PrimaryDevice::ModeSelect(cdb_t, data_out_t, int)
{
    // There is no default implementation of MODE SELECT
    throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
}

int PrimaryDevice::GetCdbByte(int index) const
{
    return controller->GetCdb()[index];
}

int PrimaryDevice::GetCdbInt16(int index) const
{
    return memory_util::GetInt16(controller->GetCdb(), index);
}

int PrimaryDevice::GetCdbInt24(int index) const
{
    return memory_util::GetInt24(controller->GetCdb(), index);
}

uint32_t PrimaryDevice::GetCdbInt32(int index) const
{
    return memory_util::GetInt32(controller->GetCdb(), index);
}

uint64_t PrimaryDevice::GetCdbInt64(int index) const
{
    return memory_util::GetInt64(controller->GetCdb(), index);
}
