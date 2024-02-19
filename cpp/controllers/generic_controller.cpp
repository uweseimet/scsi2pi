//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "buses/bus_factory.h"
#include "buses/gpio_bus.h"
#ifdef BUILD_MODE_PAGE_DEVICE
#include "devices/mode_page_device.h"
#endif
#include "generic_controller.h"

using namespace spdlog;
using namespace scsi_defs;
using namespace s2p_util;

void GenericController::Reset()
{
    AbstractController::Reset();

    initiator_id = UNKNOWN_INITIATOR_ID;
}

bool GenericController::Process(int id)
{
    GetBus().Acquire();

    if (GetBus().GetRST()) {
        LogWarn("RESET signal received");
        Reset();
        return false;
    }

    initiator_id = id;

    if (!ProcessPhase()) {
        Error(sense_key::aborted_command, asc::controller_process_phase);
        return false;
    }

    return !IsBusFree();
}

void GenericController::BusFree()
{
    if (!IsBusFree()) {
        LogTrace("BUS FREE phase");
        SetPhase(phase_t::busfree);

        GetBus().SetREQ(false);
        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(false);
        GetBus().SetBSY(false);

        // Initialize status and message
        SetStatus(status::good);
        SetMessage(0x00);

        return;
    }

    if (GetBus().GetSEL() && !GetBus().GetBSY()) {
        Selection();
    }
}

void GenericController::Selection()
{
    if (!IsSelection()) {
        LogTrace("SELECTION phase");
        SetPhase(phase_t::selection);

        GetBus().SetBSY(true);
        return;
    }

    if (!GetBus().GetSEL() && GetBus().GetBSY()) {
        // Message out phase if ATN=1, otherwise command phase
        if (GetBus().GetATN()) {
            MsgOut();
        } else {
            Command();
        }
    }
}

void GenericController::Command()
{
    if (!IsCommand()) {
        LogTrace("COMMAND phase");
        SetPhase(phase_t::command);

        GetBus().SetMSG(false);
        GetBus().SetCD(true);
        GetBus().SetIO(false);

        auto &buf = GetBuffer();

        const int actual_count = GetBus().CommandHandShake(buf);
        if (!actual_count) {
            LogTrace(fmt::format("Received unknown command: ${:02x}", buf[0]));
            Error(sense_key::illegal_request, asc::invalid_command_operation_code);
            return;
        }

        const int command_bytes_count = BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(buf[0]));
        assert(command_bytes_count <= 16);

        for (int i = 0; i < command_bytes_count; i++) {
            SetCdbByte(i, buf[i]);
        }

        // Check the log level first in order to avoid a time-consuming string construction
        if (get_level() <= level::debug) {
            LogCdb();
        }

        if (actual_count != command_bytes_count) {
            LogWarn(fmt::format("Received {0} byte(s) in COMMAND phase for command ${1:02x}, {2} required",
                command_bytes_count, GetCdbByte(0), actual_count));
            Error(sense_key::aborted_command, asc::command_phase_error);
            return;
        }

        SetCurrentLength(0);

        Execute();
    }
}

void GenericController::Execute()
{
    ResetOffset();
    SetTransferSize(0, 0);

    int lun = GetEffectiveLun();
    if (!GetDeviceForLun(lun)) {
        if (GetOpcode() != scsi_command::cmd_inquiry && GetOpcode() != scsi_command::cmd_request_sense) {
            Error(sense_key::illegal_request, asc::invalid_lun);
            return;
        }

        assert(GetDeviceForLun(0));

        lun = 0;
    }

    // SCSI-2 4.4.3 Incorrect logical unit handling
    if (GetOpcode() == scsi_command::cmd_inquiry && !GetDeviceForLun(lun)) {
        GetBuffer().data()[0] = 0x7f;
        return;
    }

    auto device = GetDeviceForLun(lun);

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (GetOpcode() != scsi_command::cmd_request_sense) {
        SetStatus(status::good);
        device->SetStatus(sense_key::no_sense, asc::no_additional_sense_information);
    }

    if (device->CheckReservation(initiator_id, GetOpcode(), GetCdbByte(4) & 0x01)) {
        try {
            device->Dispatch(GetOpcode());
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
        }
    }
    else {
        Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
    }
}

void GenericController::Status()
{
    if (!IsStatus()) {
        if (const auto &it_status = STATUS_MAPPING.find(GetStatus()); it_status != STATUS_MAPPING.end()) {
            LogTrace(fmt::format("Status phase, status is {0} (status code ${1:02x})", it_status->second,
                static_cast<int>(GetStatus())));
        }
        else {
            LogTrace(fmt::format("Status phase, status code is ${:02x}", static_cast<int>(GetStatus())));
        }

        SetPhase(phase_t::status);

        GetBus().SetMSG(false);
        GetBus().SetCD(true);
        GetBus().SetIO(true);

        ResetOffset();
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        GetBuffer()[0] = (uint8_t)GetStatus();

        return;
    }

    Send();
}

void GenericController::MsgIn()
{
    if (!IsMsgIn()) {
        LogTrace("MESSAGE IN phase");
        SetPhase(phase_t::msgin);

        GetBus().SetMSG(true);
        GetBus().SetCD(true);
        GetBus().SetIO(true);

        ResetOffset();

        return;
    }

    Send();
}

void GenericController::DataIn()
{
    if (!IsDataIn()) {
        if (!GetCurrentLength()) {
            Status();
            return;
        }

        LogTrace("DATA IN phase");
        SetPhase(phase_t::datain);

        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(true);

        ResetOffset();

        return;
    }

    Send();
}

void GenericController::DataOut()
{
    if (!IsDataOut()) {
        if (!GetCurrentLength()) {
            Status();
            return;
        }

        LogTrace("DATA OUT phase");
        SetPhase(phase_t::dataout);

        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(false);

        ResetOffset();

        return;
    }

    Receive();
}

void GenericController::Error(sense_key sense_key, asc asc, scsi_defs::status status)
{
    GetBus().Acquire();
    if (GetBus().GetRST() || IsStatus() || IsMsgIn()) {
        BusFree();
        return;
    }

    int lun = GetEffectiveLun();
    if (asc == asc::invalid_lun || !GetDeviceForLun(lun)) {
        lun = 0;
    }

    if (sense_key != sense_key::no_sense || asc != asc::no_additional_sense_information) {
        LogDebug(FormatSenseData(sense_key, asc));

        // Set Sense Key and ASC in the device for a subsequent REQUEST SENSE
        GetDeviceForLun(lun)->SetStatus(sense_key, asc);
    }

    SetStatus(status);
    SetMessage(0x00);

    Status();
}

void GenericController::Send()
{
    assert(!GetBus().GetREQ());
    assert(GetBus().GetIO());

    if (const auto length = GetCurrentLength(); length) {
        LogTrace(fmt::format("Sending {} byte(s)", length));

        // The DaynaPort delay work-around for the Mac should be taken from the respective LUN, but as there are
        // no Mac Daynaport drivers for LUNs other than 0 the current work-around is fine. The work-around is
        // required for cases where the actually requested LUN does not exist but is tested for with INQUIRY.
        if (const int l = GetBus().SendHandShake(GetBuffer().data() + GetOffset(), length,
            GetDeviceForLun(0)->GetDelayAfterBytes()); l != length) {
            if (IsDataIn()) {
                LogWarn(fmt::format("Sent {0} byte(s) in DATA IN phase, command requires {1}", l, length));
            }
            Error(sense_key::aborted_command, asc::data_phase_error);
        }
        else {
            UpdateOffsetAndLength();
        }

        return;
    }

    const bool pending_data = UpdateTransferSize();

    if (pending_data && IsDataIn() && !XferIn(GetBuffer())) {
        return;
    }

    if (pending_data) {
        assert(GetCurrentLength());
        assert(!GetOffset());
        return;
    }

    LogTrace("All data transferred");

    // Move to next phase
    switch (GetPhase()) {
    case phase_t::msgin:
        ProcessExtendedMessage();
        break;

    case phase_t::datain:
        Status();
        break;

    case phase_t::status:
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        GetBuffer()[0] = (uint8_t)GetMessage();
        MsgIn();
        break;

    default:
        assert(false);
        break;
    }
}

void GenericController::Receive()
{
    assert(!GetBus().GetREQ());
    assert(!GetBus().GetIO());

    if (const auto length = GetCurrentLength(); length) {
        LogTrace(fmt::format("Receiving {} byte(s)", length));

        if (const int l = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), length); l != length) {
            LogWarn(fmt::format("Received {0} byte(s) in DATA OUT phase, command requires {1}", l, length));
            Error(sense_key::aborted_command, asc::data_phase_error);
            return;
        }
        // Assume that data less than < 256 bytes in DATA OUT are parameters to a non block-oriented command
        else if (IsDataOut() && !GetOffset() && l < 256 && get_level() == level::trace) {
            LogTrace(fmt::format("{} byte(s) of command parameter data:\n{}", l, FormatBytes(GetBuffer(), l)));
        }
    }

    if (GetCurrentLength()) {
        UpdateOffsetAndLength();
        return;
    }

    const bool pending_data = UpdateTransferSize();

    // Processing after receiving data
    switch (GetPhase()) {
    case phase_t::dataout:
        if (!XferOut(pending_data)) {
            return;
        }
        break;

    case phase_t::msgout:
        XferMsg(GetBuffer()[0]);

        // Clear message data in preparation for MESSAGE IN
        SetMessage(0x00);
        break;

    default:
        assert(false);
        break;
    }

    if (pending_data) {
        assert(GetCurrentLength());
        assert(!GetOffset());
        return;
    }

    // Move to next phase
    switch (GetPhase()) {
    case phase_t::msgout:
        ProcessMessage();
        break;

    case phase_t::dataout:
        // All data have been transferred
        Status();
        break;

    default:
        error("Unexpected bus phase: " + Bus::GetPhaseName(GetPhase()));
        assert(false);
        break;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool GenericController::XferIn(vector<uint8_t> &buf)
{
    // Limited to read commands
    switch (GetOpcode()) {
    case scsi_command::cmd_read6:
        case scsi_command::cmd_read10:
        case scsi_command::cmd_read16:
        case scsi_command::cmd_read_long10:
        case scsi_command::cmd_read_capacity16_read_long16:
        try {
            SetCurrentLength(GetDeviceForLun(GetEffectiveLun())->ReadData(buf));
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
            return false;
        }

        ResetOffset();
        return true;

    default:
        assert(false);
        break;
    }

    Error(sense_key::aborted_command, asc::controller_xfer_in);

    return false;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool GenericController::XferOut(bool cont)
{
    auto device = GetDeviceForLun(GetEffectiveLun());

    // Limited to write/verify commands
    switch (const auto opcode = GetOpcode(); opcode) {
    case scsi_command::cmd_mode_select6:
        case scsi_command::cmd_mode_select10:
        {
#ifdef BUILD_MODE_PAGE_DEVICE
        auto mode_page_device = dynamic_pointer_cast<ModePageDevice>(device);
        assert(mode_page_device);
        if (!mode_page_device) {
            Error(sense_key::aborted_command, asc::controller_xfer_out);
            return false;
        }

        try {
            mode_page_device->ModeSelect(GetOpcode(), GetCdb(), GetBuffer(), GetOffset());
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
            return false;
        }

        return true;
#endif
    }
        break;

    case scsi_command::cmd_write6:
        case scsi_command::cmd_write10:
        case scsi_command::cmd_write16:
        case scsi_command::cmd_verify10:
        case scsi_command::cmd_verify16:
        case scsi_command::cmd_write_long10:
        case scsi_command::cmd_write_long16:
        case scsi_command::cmd_execute_operation:
        {
        try {
            const auto length = device->WriteData(GetBuffer(), opcode);

            if (cont) {
                SetCurrentLength(length);
                ResetOffset();
            }
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
            return false;
        }

        return true;
    }
        break;

    default:
        assert(false);
        break;
    }

    Error(sense_key::aborted_command, asc::controller_xfer_out);

    return false;
}
#pragma GCC diagnostic pop

void GenericController::LogCdb() const
{
    const string &command_name = BusFactory::Instance().GetCommandName(GetOpcode());
    string s = fmt::format("Controller is executing {}, CDB $",
        !command_name.empty() ? command_name : fmt::format("{:02x}", GetCdbByte(0)));
    for (int i = 0; i < BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(GetCdbByte(0))); i++) {
        if (i) {
            s += ":";
        }
        s += fmt::format("{:02x}", GetCdbByte(i));
    }
    LogDebug(s);
}
