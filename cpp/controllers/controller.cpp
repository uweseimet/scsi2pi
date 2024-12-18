//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controller.h"
#include "buses/bus_factory.h"
#include "base/primary_device.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace s2p_util;

void Controller::Reset()
{
    AbstractController::Reset();

    identified_lun = -1;

    atn_msg = false;
}

bool Controller::Process()
{
    GetBus().Acquire();

    if (GetBus().GetRST()) {
        LogWarn("Received RESET signal");
        Reset();
        return false;
    }

    if (!ProcessPhase()) {
        Error(sense_key::aborted_command, asc::controller_process_phase);
        return false;
    }

    return !IsBusFree();
}

void Controller::BusFree()
{
    if (!IsBusFree()) {
        LogTrace("BUS FREE phase");
        SetPhase(bus_phase::busfree);

        GetBus().SetREQ(false);
        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(false);
        GetBus().SetBSY(false);

        SetStatus(status_code::good);

        identified_lun = -1;

        atn_msg = false;

        return;
    }

    if (GetBus().GetSEL() && !GetBus().GetBSY()) {
        Selection();
    }
}

void Controller::Selection()
{
    if (!IsSelection()) {
        LogTrace("SELECTION phase");
        SetPhase(bus_phase::selection);

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

void Controller::Command()
{
    if (!IsCommand()) {
        LogTrace("COMMAND phase");
        SetPhase(bus_phase::command);

        GetBus().SetMSG(false);
        GetBus().SetCD(true);
        GetBus().SetIO(false);

        auto &buf = GetBuffer();

        const int actual_count = GetBus().CommandHandShake(buf);
        if (actual_count <= 0) {
            if (!actual_count) {
                LogDebug(fmt::format("Controller received unknown command: ${:02x}", buf[0]));
                Error(sense_key::illegal_request, asc::invalid_command_operation_code);
            }
            else {
                Error(sense_key::aborted_command, asc::command_phase_error);
            }
            return;
        }

        const int command_bytes_count = BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(buf[0]));
        assert(command_bytes_count && command_bytes_count <= static_cast<int>(GetCdb().size()));

        for (int i = 0; i < command_bytes_count; i++) {
            SetCdbByte(i, buf[i]);
        }

        // Check the log level in order to avoid an unnecessary time-consuming string construction
        if (get_level() <= level::debug) {
            LogCdb();
        }

        if (actual_count != command_bytes_count) {
            LogWarn(fmt::format("Received {0} byte(s) in COMMAND phase for command ${1:02x}, {2} required",
                command_bytes_count, GetCdb()[0], actual_count));
            Error(sense_key::aborted_command, asc::command_phase_error);
            return;
        }

        const int control = GetCdb()[command_bytes_count - 1];
        linked = control & 0x01;
        flag = control & 0x02;

        if (flag && !linked) {
            Error(sense_key::illegal_request, asc::invalid_field_in_cdb);
            return;
        }

        Execute();
    }
}

void Controller::Execute()
{
    SetCurrentLength(0);
    ResetOffset();
    SetTransferSize(0, 0);

    const auto opcode = static_cast<scsi_command>(GetCdb()[0]);

    auto device = GetDeviceForLun(GetEffectiveLun());
    if (!device) {
        if (opcode != scsi_command::inquiry && opcode != scsi_command::request_sense) {
            Error(sense_key::illegal_request, asc::invalid_lun);
            return;
        }

        device = GetDeviceForLun(0);
        assert(device);
    }

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (opcode != scsi_command::request_sense) {
        SetStatus(status_code::good);
        device->SetStatus(sense_key::no_sense, asc::no_additional_sense_information);
    }

    if (device->CheckReservation(GetInitiatorId())) {
        try {
            device->Dispatch(opcode);
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
        }
    }
}

void Controller::Status()
{
    if (IsStatus()) {
        Send();
        return;
    }

    LogTrace(fmt::format("Status phase, status is {0} (status code ${1:02x})", STATUS_MAPPING.at(GetStatus()),
        static_cast<int>(GetStatus())));

    SetPhase(bus_phase::status);

    GetBus().SetMSG(false);
    GetBus().SetCD(true);
    GetBus().SetIO(true);

    ResetOffset();
    SetCurrentLength(1);
    SetTransferSize(1, 1);

    // If this is a successfully terminated linked command convert the status code
    GetBuffer()[0] =
        linked && GetStatus() == status_code::good ? (uint8_t)status_code::intermediate : (uint8_t)GetStatus();
}

void Controller::MsgIn()
{
    if (IsMsgIn()) {
        Send();
        return;
    }

    LogTrace("MESSAGE IN phase");
    SetPhase(bus_phase::msgin);

    GetBus().SetMSG(true);
    GetBus().SetCD(true);
    GetBus().SetIO(true);

    ResetOffset();
}

void Controller::MsgOut()
{
    if (IsMsgOut()) {
        Receive();
        return;
    }

    LogTrace("MESSAGE OUT phase");

    // Process the IDENTIFY message
    if (IsSelection()) {
        atn_msg = true;
        msg_bytes.clear();
    }

    SetPhase(bus_phase::msgout);

    GetBus().SetMSG(true);
    GetBus().SetCD(true);
    GetBus().SetIO(false);

    ResetOffset();
    SetCurrentLength(1);
    SetTransferSize(1, 1);
}

void Controller::DataIn()
{
    if (IsDataIn()) {
        Send();
        return;
    }

    if (!GetCurrentLength()) {
        Status();
        return;
    }

    LogTrace("DATA IN phase");
    SetPhase(bus_phase::datain);

    GetBus().SetMSG(false);
    GetBus().SetCD(false);
    GetBus().SetIO(true);

    ResetOffset();
}

void Controller::DataOut()
{
    if (IsDataOut()) {
        Receive();
        return;
    }

    if (!GetCurrentLength()) {
        Status();
        return;
    }

    LogTrace("DATA OUT phase");
    SetPhase(bus_phase::dataout);

    GetBus().SetMSG(false);
    GetBus().SetCD(false);
    GetBus().SetIO(false);

    ResetOffset();
}

void Controller::Error(sense_key sense_key, asc asc, status_code status)
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

    Status();
}

void Controller::Send()
{
    assert(!GetBus().GetREQ());
    assert(GetBus().GetIO());

    if (const auto length = GetCurrentLength(); length) {
        // Assume that data less than < 256 bytes in DATA IN are data for a non block-oriented command
        if (length < 256 && get_level() == level::trace) {
            LogTrace(fmt::format("Sending {0} byte(s):\n{1}", length, FormatBytes(GetBuffer(), length)));
        }
        else {
            LogTrace(fmt::format("Sending {} byte(s)", length));
        }

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

    if (pending_data && IsDataIn() && !XferIn()) {
        return;
    }

    if (pending_data) {
        assert(GetCurrentLength());
        assert(!GetOffset());
        return;
    }

    LogTrace("All data transferred");

    switch (GetPhase()) {
    case bus_phase::msgin:
        ProcessEndOfMessage();
        break;

    case bus_phase::datain:
        Status();
        break;

    case bus_phase::status:
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        // Message byte
        if (linked) {
            GetBuffer()[0] = static_cast<uint8_t>(
                flag ? message_code::linked_command_complete_with_flag : message_code::linked_command_complete);
        }
        else {
            GetBuffer()[0] = static_cast<uint8_t>(message_code::command_complete);
        }
        MsgIn();
        break;

    default:
        assert(false);
        break;
    }
}

void Controller::Receive()
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
            LogTrace(fmt::format("{0} byte(s) of command parameter data:\n{1}", l, FormatBytes(GetBuffer(), l)));
        }
    }

    if (GetCurrentLength()) {
        UpdateOffsetAndLength();
        return;
    }

    const bool pending_data = UpdateTransferSize();

    // Processing after receiving data
    switch (GetPhase()) {
    case bus_phase::dataout:
        if (!XferOut(pending_data)) {
            return;
        }
        break;

    case bus_phase::msgout:
        XferMsg();
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

    switch (GetPhase()) {
    case bus_phase::dataout:
        // All data have been transferred
        Status();
        break;

    case bus_phase::msgout:
        ProcessMessage();
        break;

    default:
        assert(false);
        break;
    }
}

bool Controller::XferIn()
{
    // Limited to read commands with DATA IN phase
    assert(static_cast<scsi_command>(GetCdb()[0]) == scsi_command::read6 ||
        static_cast<scsi_command>(GetCdb()[0]) == scsi_command::read10 ||
        static_cast<scsi_command>(GetCdb()[0]) == scsi_command::read16 ||
        static_cast<scsi_command>(GetCdb()[0]) == scsi_command::get_message6 ||
        static_cast<scsi_command>(GetCdb()[0]) == scsi_command::read_capacity16_read_long16);

    try {
        SetCurrentLength(GetDeviceForLun(GetEffectiveLun())->ReadData(GetBuffer()));
    }
    catch (const scsi_exception &e) {
        Error(e.get_sense_key(), e.get_asc());
        return false;
    }

    ResetOffset();
    return true;
}

bool Controller::XferOut(bool pending_data)
{
    const auto device = GetDeviceForLun(GetEffectiveLun());
    try {
        switch (const auto opcode = static_cast<scsi_command>(GetCdb()[0]); opcode) {
        case scsi_command::mode_select6:
        case scsi_command::mode_select10:
            device->ModeSelect(GetCdb(), GetBuffer(), GetOffset());
            break;

        case scsi_command::write6:
        case scsi_command::write10:
        case scsi_command::write16:
        case scsi_command::verify10:
        case scsi_command::verify16:
        case scsi_command::write_long10:
        case scsi_command::write_long16:
        case scsi_command::execute_operation: {
            const auto length = device->WriteData(GetBuffer(), opcode);
            if (pending_data) {
                SetCurrentLength(length);
                ResetOffset();
            }
            break;
        }

        default:
            // Limited to write/verify/mode commands with DATA OUT phase
            assert(false);
            return false;
        }
    }
    catch (const scsi_exception &e) {
        Error(e.get_sense_key(), e.get_asc());
        return false;
    }

    return true;
}

void Controller::XferMsg()
{
    assert(IsMsgOut());

    if (atn_msg) {
        msg_bytes.emplace_back(GetBuffer()[0]);
    }
}

void Controller::ParseMessage()
{
    for (const uint8_t message : msg_bytes) {
        switch (message) {
        case 0x01: {
            LogTrace("Received EXTENDED MESSAGE");
            SetCurrentLength(1);
            SetTransferSize(1, 1);
            // MESSSAGE REJECT
            GetBuffer()[0] = 0x07;
            MsgIn();
            return;
        }

        case static_cast<uint8_t>(message_code::abort): {
            LogTrace("Received ABORT message");
            BusFree();
            return;
        }

        case static_cast<uint8_t>(message_code::bus_device_reset): {
            LogTrace("Received BUS DEVICE RESET message");
            if (const auto device = GetDeviceForLun(GetEffectiveLun()); device) {
                device->SetReset(true);
                device->DiscardReservation();
            }
            BusFree();
            return;
        }

        default:
            if (message >= 0x80) {
                identified_lun = static_cast<int>(message) & 0x1f;
                LogTrace("Received IDENTIFY message for LUN " + to_string(identified_lun));
            }
            break;
        }
    }
}

void Controller::ProcessMessage()
{
    // MESSAGE OUT phase as long as ATN is asserted
    if (GetBus().GetATN()) {
        ResetOffset();
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        return;
    }

    if (atn_msg) {
        atn_msg = false;
        ParseMessage();
    }

    Command();
}

void Controller::ProcessEndOfMessage()
{
    // Completed sending response to extended message of IDENTIFY message or executing a linked command
    if (atn_msg || linked) {
        atn_msg = false;
        linked = false;
        flag = false;
        Command();
    } else {
        BusFree();
    }
}

int Controller::GetEffectiveLun() const
{
    // Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
    return identified_lun != -1 ? identified_lun : GetCdb()[1] >> 5;
}

void Controller::LogCdb() const
{
    const auto opcode = static_cast<scsi_command>(GetCdb()[0]);
    const string_view &command_name = BusFactory::Instance().GetCommandName(opcode);
    string s = fmt::format("Controller is executing {}, CDB ",
        !command_name.empty() ? command_name : fmt::format("{:02x}", GetCdb()[0]));
    for (int i = 0; i < BusFactory::Instance().GetCommandBytesCount(opcode); i++) {
        if (i) {
            s += ":";
        }
        s += fmt::format("{:02x}", GetCdb()[i]);
    }
    LogDebug(s);
}
