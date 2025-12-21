//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controller.h"
#include "base/primary_device.h"
#include "buses/bus.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"
#include "script_generator.h"

using namespace spdlog;
using namespace s2p_util;

void Controller::Reset()
{
    AbstractController::Reset();

    bus.Reset();

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
    bus.Acquire();

    if (bus.GetRST()) {
        LogWarn("Received RESET signal");
        Reset();
        return false;
    }

    // TODO Catch ScsiException here instead of everywhere else and call Error()?
    if (!ProcessPhase()) {
        Error(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
        return false;
    }

    return !IsBusFree();
}

void Controller::BusFree()
{
    if (!IsBusFree()) {
        LogTrace("BUS FREE phase");
        SetPhase(BusPhase::BUS_FREE);

        bus.SetREQ(false);
        bus.SetMSG(false);
        bus.SetCD(false);
        bus.SetIO(false);
        bus.SetBSY(false);

        SetStatus(StatusCode::GOOD);

        identified_lun = -1;

        atn_msg = false;

        return;
    }

    if (bus.GetSEL() && !bus.GetBSY()) {
        Selection();
    }
}

void Controller::Selection()
{
    if (!IsSelection()) {
        LogTrace("SELECTION phase");
        SetPhase(BusPhase::SELECTION);

        bus.SetBSY(true);
        return;
    }

    if (!bus.GetSEL() && bus.GetBSY()) {
        // Message out phase if ATN=1, otherwise command phase
        if (bus.GetATN()) {
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
        SetPhase(BusPhase::COMMAND);

        bus.SetMSG(false);
        bus.SetCD(true);
        bus.SetIO(false);

        auto &buf = GetBuffer();

        const int actual_count = bus.TargetCommandHandShake(buf);
        if (actual_count <= 0) {
            if (!actual_count) {
                LogDebug(fmt::format("Controller received unknown command: ${:02x}", buf[0]));
                RaiseDeferredError(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_COMMAND_OPERATION_CODE);
            }
            else {
                bus.SetRST(true);
                RaiseDeferredError(SenseKey::ABORTED_COMMAND, Asc::COMMAND_PHASE_ERROR);
            }
            return;
        }

        const int command_bytes_count = CommandMetaData::GetInstance().GetByteCount(
            static_cast<ScsiCommand>(buf[0]));
        assert(command_bytes_count && command_bytes_count <= static_cast<int>(GetCdb().size()));

        for (int i = 0; i < command_bytes_count; ++i) {
            SetCdbByte(i, buf[i]);
        }

        if (script_generator) {
            script_generator->AddCdb(GetTargetId(), GetEffectiveLun(), GetCdb());
        }

        // Check the log level in order to avoid an unnecessary time-consuming string construction
        if (GetLogger().level() <= level::debug) {
            LogDebug(CommandMetaData::GetInstance().LogCdb(span(buf.data(), command_bytes_count), "Controller"));
        }

        if (actual_count != command_bytes_count) {
            LogWarn(fmt::format("Received {0} byte(s) in COMMAND phase for command ${1:02x}, {2} required",
                command_bytes_count, GetCdb()[0], actual_count));
            bus.SetRST(true);
            RaiseDeferredError(SenseKey::ABORTED_COMMAND, Asc::COMMAND_PHASE_ERROR);
            return;
        }

        const auto control = GetCdb()[command_bytes_count - 1];
        linked = control & 0x01;
        flag = control & 0x02;

        if (flag && !linked) {
            RaiseDeferredError(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
            return;
        }

        // Ensure correct sense data if the previous command was rejected by the controller and not by the device
        if (deferred_sense_key != SenseKey::NO_SENSE
            && static_cast<ScsiCommand>(GetCdb()[0]) == ScsiCommand::REQUEST_SENSE) {
            ProvideSenseData();
            return;
        }
        deferred_sense_key = SenseKey::NO_SENSE;
        deferred_asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION;

        Execute();
    }
}

void Controller::Execute()
{
    SetCurrentLength(0);
    ResetOffset();
    SetTransferSize(0, 0);

    const auto opcode = static_cast<ScsiCommand>(GetCdb()[0]);

    auto device = GetDeviceForLun(GetEffectiveLun());
    if (!device) {
        if (opcode != ScsiCommand::INQUIRY && opcode != ScsiCommand::REQUEST_SENSE) {
            Error(SenseKey::ILLEGAL_REQUEST, Asc::LOGICAL_UNIT_NOT_SUPPORTED);
            return;
        }

        device = GetDeviceForLun(0);
        assert(device);
    }

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (opcode != ScsiCommand::REQUEST_SENSE) {
        SetStatus(StatusCode::GOOD);
        device->ResetStatus();
    }

    if (device->CheckReservation(GetInitiatorId())) {
        try {
            device->Dispatch(opcode);
        }
        catch (const ScsiException &e) {
            Error(e.GetSenseKey(), e.GetAsc());
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

    SetPhase(BusPhase::STATUS);

    bus.SetMSG(false);
    bus.SetCD(true);
    bus.SetIO(true);

    ResetOffset();
    SetCurrentLength(1);
    SetTransferSize(1, 1);

    // If this is a successfully terminated linked command convert the status code
    GetBuffer()[0] =
        linked && GetStatus() == StatusCode::GOOD ?
            static_cast<uint8_t>(StatusCode::INTERMEDIATE) : static_cast<uint8_t>(GetStatus());
}

void Controller::MsgIn()
{
    if (IsMsgIn()) {
        Send();
        return;
    }

    LogTrace("MESSAGE IN phase");
    SetPhase(BusPhase::MSG_IN);

    bus.SetMSG(true);
    bus.SetCD(true);
    bus.SetIO(true);

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

    SetPhase(BusPhase::MSG_OUT);

    bus.SetMSG(true);
    bus.SetCD(true);
    bus.SetIO(false);

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
    SetPhase(BusPhase::DATA_IN);

    bus.SetMSG(false);
    bus.SetCD(false);
    bus.SetIO(true);

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
    // Current length == -1 enforces a DATA OUT phase, in particular for FORMAT UNIT with the SG 3 driver
    else if (GetCurrentLength() == -1) {
        SetCurrentLength(0);
    }

    LogTrace("DATA OUT phase");
    SetPhase(BusPhase::DATA_OUT);

    bus.SetMSG(false);
    bus.SetCD(false);
    bus.SetIO(false);

    ResetOffset();
}

void Controller::Error(SenseKey sense_key, Asc asc, StatusCode status_code)
{
    bus.Acquire();
    if (bus.GetRST() || IsStatus() || IsMsgIn()) {
        BusFree();
        return;
    }

    int lun = GetEffectiveLun();
    if (asc == Asc::LOGICAL_UNIT_NOT_SUPPORTED || !GetDeviceForLun(lun)) {
        lun = 0;
    }

    if (sense_key != SenseKey::NO_SENSE || asc != Asc::NO_ADDITIONAL_SENSE_INFORMATION) {
        LogDebug(FormatSenseData(sense_key, asc));

        // Set Sense Key and ASC in the device for a subsequent REQUEST SENSE
        GetDeviceForLun(lun)->SetStatus(sense_key, asc);
    }

    SetStatus(status_code);

    Status();
}

void Controller::Send()
{
    assert(!bus.GetREQ());
    assert(bus.GetIO());

    if (const auto length = GetCurrentLength(); length) {
        if (GetLogger().level() == level::trace && IsDataIn()) {
            const string &bytes = FormatBytes(GetBuffer(), length);
            LogTrace(fmt::format("Sending {0} byte(s) at offset {1} in DATA IN phase{2}{3}", length, GetOffset(),
                bytes.empty() ? "" : ":\n", bytes));
        }

        // The DaynaPort delay work-around for the Mac should be taken from the respective LUN, but as there are
        // no Mac Daynaport drivers for LUNs other than 0 the current work-around is fine. The work-around is
        // required for cases where the actually requested LUN does not exist but is tested for with INQUIRY.
        if (const int l = bus.TargetSendHandShake(span(GetBuffer().data() + GetOffset(), length),
            GetDeviceForLun(0)->GetDelayAfterBytes()); l != length) {
            LogWarn(fmt::format("Sent {0} byte(s), {1} required", l, length));
            bus.SetRST(true);
            Error(SenseKey::ABORTED_COMMAND, Asc::DATA_PHASE_ERROR);
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
    case BusPhase::MSG_IN:
        ProcessEndOfMessage();
        break;

    case BusPhase::DATA_IN:
        Status();
        break;

    case BusPhase::STATUS:
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        // Message byte
        if (linked) {
            GetBuffer()[0] = static_cast<uint8_t>(
                flag ? MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG : MessageCode::LINKED_COMMAND_COMPLETE);
        }
        else {
            GetBuffer()[0] = static_cast<uint8_t>(MessageCode::COMMAND_COMPLETE);
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
    assert(!bus.GetREQ());
    assert(!bus.GetIO());

    if (const auto curr_length = GetCurrentLength(); curr_length) {
        if (!IsMsgOut()) {
            LogTrace(fmt::format("Receiving {0} byte(s) at offset {1}", curr_length, GetOffset()));
        }

        if (const int l = bus.TargetReceiveHandShake(span(GetBuffer().data() + GetOffset(), curr_length)); l
            != curr_length) {
            LogWarn(fmt::format("Received {0} byte(s), {1} required", l, curr_length));
            bus.SetRST(true);
            Error(SenseKey::ABORTED_COMMAND, Asc::DATA_PHASE_ERROR);
            return;
        }

        if (GetLogger().level() == level::trace && IsDataOut()) {
            const string &bytes = FormatBytes(GetBuffer(), curr_length);
            LogTrace(
                fmt::format("Received {0} byte(s) in DATA OUT phase{1}{2}", curr_length, bytes.empty() ? "" : ":\n", bytes));
        }

        if (IsDataOut() && script_generator) {
            script_generator->AddData(span(GetBuffer().data() + GetOffset(), curr_length));
        }

        UpdateOffsetAndLength();
        return;
    }

    const int length = GetChunkSize() < GetRemainingLength() ? GetChunkSize() : GetRemainingLength();

    // Processing after receiving data
    switch (GetPhase()) {
    case BusPhase::DATA_OUT:
        if (!TransferFromHost(length)) {
            return;
        }
        break;

    case BusPhase::MSG_OUT:
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
    case BusPhase::DATA_OUT:
        // All data have been transferred
        Status();
        break;

    case BusPhase::MSG_OUT:
        ProcessMessage();
        break;

    default:
        assert(false);
        break;
    }
}

void Controller::TransferToHost()
{
    assert(!CommandMetaData::GetInstance().GetCdbMetaData(static_cast<ScsiCommand>(GetCdb()[0])).has_data_out);

    try {
        GetDeviceForLun(GetEffectiveLun())->ReadData(GetBuffer());
        if (GetRemainingLength()) {
            SetCurrentLength(GetRemainingLength() < GetChunkSize() ? GetRemainingLength() : GetChunkSize());
            ResetOffset();
        }
    }
    catch (const ScsiException &e) {
        Error(e.GetSenseKey(), e.GetAsc());
    }
}

bool Controller::TransferFromHost(int length)
{
    const auto cmd = static_cast<ScsiCommand>(GetCdb()[0]);
    assert(CommandMetaData::GetInstance().GetCdbMetaData(static_cast<ScsiCommand>(GetCdb()[0])).has_data_out);

    int transferred_length = length;
    const auto device = GetDeviceForLun(GetEffectiveLun());
    try {
        if ((cmd == ScsiCommand::MODE_SELECT_6 || cmd == ScsiCommand::MODE_SELECT_10) && device->GetType() != SCSG) {
            // The offset is the number of bytes transferred, i.e. the length of the parameter list
            device->ModeSelect(GetCdb(), GetBuffer(), GetOffset());
        }
        else {
            transferred_length = device->WriteData(GetCdb(), GetBuffer(), GetOffset(), length);
        }
    }
    catch (const ScsiException &e) {
        Error(e.GetSenseKey(), e.GetAsc());
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
        const auto msg = GetBuffer()[0];
        msg_bytes.emplace_back(msg);

        // Do not log IDENTIFY message twice
        if (msg < 0x80) {
            LogTrace(fmt::format("Received message byte ${:02x}", msg));
        }
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

        case static_cast<uint8_t>(MessageCode::ABORT): {
            LogTrace("Received ABORT message");
            BusFree();
            return;
        }

        case static_cast<uint8_t>(MessageCode::BUS_DEVICE_RESET): {
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
    if (bus.GetATN()) {
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

void Controller::RaiseDeferredError(SenseKey s, Asc a)
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

    deferred_sense_key = SenseKey::NO_SENSE;
    deferred_asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION;

    DataIn();
}

int Controller::GetEffectiveLun() const
{
    // Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
    return identified_lun != -1 ? identified_lun : GetCdb()[1] >> 5;
}
