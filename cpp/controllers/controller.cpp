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
#include "base/primary_device.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace s2p_util;

void Controller::Reset()
{
    AbstractController::Reset();

    identified_lun = -1;

    ResetFlags();
}

void Controller::ResetFlags()
{
    linked = false;
    flag = false;
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

    // TODO Catch scsi_exception here instead of everywhere else and call Error()?
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
                RaiseDeferredError(sense_key::illegal_request, asc::invalid_command_operation_code);
            }
            else {
                GetBus().SetRST(true);
                RaiseDeferredError(sense_key::aborted_command, asc::command_phase_error);
            }
            return;
        }

        const int command_bytes_count = CommandMetaData::Instance().GetCommandBytesCount(
            static_cast<scsi_command>(buf[0]));
        assert(command_bytes_count && command_bytes_count <= static_cast<int>(GetCdb().size()));

        for (int i = 0; i < command_bytes_count; i++) {
            SetCdbByte(i, buf[i]);
        }
        AddCdbToScript();

        // Check the log level in order to avoid an unnecessary time-consuming string construction
        if (get_level() <= level::debug) {
            LogDebug(CommandMetaData::Instance().LogCdb(span(buf.data(), command_bytes_count), "Controller"));
        }

        if (actual_count != command_bytes_count) {
            LogWarn(fmt::format("Received {0} byte(s) in COMMAND phase for command ${1:02x}, {2} required",
                command_bytes_count, GetCdb()[0], actual_count));
            GetBus().SetRST(true);
            RaiseDeferredError(sense_key::aborted_command, asc::command_phase_error);
            return;
        }

        const auto control = GetCdb()[command_bytes_count - 1];
        linked = control & 0x01;
        flag = control & 0x02;

        if (flag && !linked) {
            RaiseDeferredError(sense_key::illegal_request, asc::invalid_field_in_cdb);
            return;
        }

        // Ensure correct sense data if the previous command was rejected by the controller and not by the device
        if (deferred_sense_key != sense_key::no_sense
            && static_cast<scsi_command>(GetCdb()[0]) == scsi_command::request_sense) {
            ProvideSenseData();
            return;
        }
        deferred_sense_key = sense_key::no_sense;
        deferred_asc = asc::no_additional_sense_information;

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
            Error(sense_key::illegal_request, asc::logical_unit_not_supported);
            return;
        }

        device = GetDeviceForLun(0);
        assert(device);
    }

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (opcode != scsi_command::request_sense) {
        SetStatus(status_code::good);
        device->ResetStatus();
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

    LogTrace(fmt::format("STATUS phase, status is {0} (status code ${1:02x})", STATUS_MAPPING.at(GetStatus()),
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
        linked && GetStatus() == status_code::good ?
            static_cast<uint8_t>(status_code::intermediate) : static_cast<uint8_t>(GetStatus());
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
    if (asc == asc::logical_unit_not_supported || !GetDeviceForLun(lun)) {
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
        if (get_level() == level::trace && IsDataIn()) {
            LogTrace(fmt::format("Sending {0} byte(s) at offset {1} in DATA IN phase:\n{2}", length, GetOffset(),
                FormatBytes(GetBuffer(), length)));
        }

        // The DaynaPort delay work-around for the Mac should be taken from the respective LUN, but as there are
        // no Mac Daynaport drivers for LUNs other than 0 the current work-around is fine. The work-around is
        // required for cases where the actually requested LUN does not exist but is tested for with INQUIRY.
        if (const int l = GetBus().SendHandShake(GetBuffer().data() + GetOffset(), length,
            GetDeviceForLun(0)->GetDelayAfterBytes()); l != length) {
            LogWarn(fmt::format("Sent {0} byte(s), {1} required", l, length));
            GetBus().SetRST(true);
            Error(sense_key::aborted_command, asc::data_phase_error);
            return;
        }

        UpdateOffsetAndLength();
        return;
    }

    UpdateTransferLength(GetChunkSize());

    if (GetRemainingLength()) {
        if (IsDataIn()) {
            TransferToHost();
        }

        return;
    }

    // All data have been transferred

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
        if (!IsMsgOut()) {
            LogTrace(fmt::format("Receiving {0} byte(s) at offset {1}", length, GetOffset()));
        }

        if (const int l = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), length); l != length) {
            LogWarn(fmt::format("Received {0} byte(s), {1} required", l, length));
            GetBus().SetRST(true);
            Error(sense_key::aborted_command, asc::data_phase_error);
            return;
        }

        if (get_level() == level::trace && IsDataOut()) {
            LogTrace(
                fmt::format("Received {0} byte(s) in DATA OUT phase:\n{1}", length, FormatBytes(GetBuffer(), length)));
        }

        if (IsDataOut()) {
            AddDataToScript(span(GetBuffer().data() + GetOffset(), length));
        }

        UpdateOffsetAndLength();
        return;
    }

    const int length = GetChunkSize() < GetRemainingLength() ? GetChunkSize() : GetRemainingLength();

    // Processing after receiving data
    switch (GetPhase()) {
    case bus_phase::dataout:
        if (!TransferFromHost(length)) {
            return;
        }
        break;

    case bus_phase::msgout:
        UpdateTransferLength(length);
        XferMsg();
        break;

    default:
        assert(false);
        break;
    }

    if (GetRemainingLength()) {
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

void Controller::TransferToHost()
{
    assert(!CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(GetCdb()[0])).has_data_out);

    try {
        GetDeviceForLun(GetEffectiveLun())->ReadData(GetBuffer());
        if (GetRemainingLength()) {
            SetCurrentLength(GetRemainingLength() < GetChunkSize() ? GetRemainingLength() : GetChunkSize());
            ResetOffset();
        }
    }
    catch (const scsi_exception &e) {
        Error(e.get_sense_key(), e.get_asc());
    }
}

bool Controller::TransferFromHost(int length)
{
    const auto cmd = static_cast<scsi_command>(GetCdb()[0]);
    assert(CommandMetaData::Instance().GetCdbMetaData(static_cast<scsi_command>(GetCdb()[0])).has_data_out);

    int transferred_length = length;
    const auto device = GetDeviceForLun(GetEffectiveLun());
    try {
        // TODO Try to remove this special case
        if (cmd == scsi_command::mode_select_6 || cmd == scsi_command::mode_select_10) {
            // The offset is the number of bytes transferred, i.e. the length of the parameter list
            device->ModeSelect(GetCdb(), GetBuffer(), GetOffset(), 0);
        }
        else {
            transferred_length = device->WriteData(GetCdb(), GetBuffer(), GetOffset(), length);
        }
    }
    catch (const scsi_exception &e) {
        Error(e.get_sense_key(), e.get_asc());
        return false;
    }

    UpdateTransferLength(transferred_length);
    SetCurrentLength(GetChunkSize());
    ResetOffset();

    return true;
}

void Controller::XferMsg()
{
    assert(IsMsgOut());

    if (atn_msg) {
        msg_bytes.emplace_back(GetBuffer()[0]);

        LogTrace(fmt::format("Received message byte ${:02x}", GetBuffer()[0]));
    }
}

void Controller::ParseMessage()
{
    bool extended = false;
    for (const uint8_t msg_byte : msg_bytes) {
        if (extended) {
            switch (msg_byte) {
            case 0x00:
                LogTrace("Rejecting MODIFY DATA POINTERS message");
                break;

            case 0x01:
                LogTrace("Rejecting SYNCHRONOUS DATA TRANFER REQUEST message");
                break;

            case 0x03:
                LogTrace("Rejecting WIDE DATA TRANFER REQUEST message");
                break;

            case 0x04:
                LogTrace("Rejecting PARALLEL PROTOCOL REQUEST message");
                break;

            case 0x05:
                LogTrace("Rejecting MODIFY BIDIRECTIONAL DATA POINTER message");
                break;

            default:
                LogTrace(fmt::format("Rejecting extended message ${:02x}", msg_byte));
                break;
            }

            extended = false;
            SetCurrentLength(1);
            SetTransferSize(1, 1);
            // MESSSAGE REJECT
            GetBuffer()[0] = 0x07;
            MsgIn();
            return;
        }

        switch (msg_byte) {
        case 0x01: {
            extended = true;
            break;
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
            if (msg_byte >= 0x80) {
                identified_lun = static_cast<int>(msg_byte) & 0x1f;
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
    // Completed sending response to extended message or IDENTIFY message or executing a linked command
    if (atn_msg || linked) {
        ResetFlags();
        Command();
    } else {
        BusFree();
    }
}

void Controller::RaiseDeferredError(sense_key s, asc a)
{
    deferred_sense_key = s;
    deferred_asc = a;
    Error(s, a);
}

void Controller::ProvideSenseData()
{
    SetCurrentLength(18);

    auto &buf = GetBuffer();
    fill_n(buf.begin(), 18, 0);
    buf[0] = 0x70;
    buf[2] = static_cast<uint8_t>(deferred_sense_key);
    buf[7] = 10;
    buf[12] = static_cast<uint8_t>(deferred_asc);

    deferred_sense_key = sense_key::no_sense;
    deferred_asc = asc::no_additional_sense_information;

    DataIn();
}

int Controller::GetEffectiveLun() const
{
    // Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
    return identified_lun != -1 ? identified_lun : GetCdb()[1] >> 5;
}
