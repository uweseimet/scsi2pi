//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2026 Uwe Seimet
//
// This controller supports both the SCSI and SASI protocols
//
//---------------------------------------------------------------------------

#include "controller.h"
#include "buses/bus.h"
#include "devices/primary_device.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"
#include "shared/scsi_util.h"
#include "script_generator.h"

using namespace spdlog;
using namespace s2p_util;
using namespace scsi_util;

void Controller::Reset()
{
    AbstractController::Reset();

    bus.Reset();

    identified_lun = -1;

    deferred_sense_key = NO_SENSE;
    deferred_asc = NO_ADDITIONAL_SENSE_INFORMATION;

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
        Error(ABORTED_COMMAND, INTERNAL_TARGET_FAILURE);
        return false;
    }

    return !IsBusFree();
}

void Controller::BusFree()
{
    if (!IsBusFree()) {
        SetPhase(BusPhase::BUS_FREE, "BUS FREE phase");

        bus.SetREQ(false);
        bus.SetMSG(false);
        bus.SetCD(false);
        bus.SetIO(false);
        bus.SetBSY(false);

        SetStatus(GOOD);

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
        SetPhase(BusPhase::SELECTION, "SELECTION phase");

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
        SetPhase(BusPhase::COMMAND, "COMMAND phase");

        bus.SetMSG(false);
        bus.SetCD(true);
        bus.SetIO(false);

        auto &buf = GetBuffer();

        const int actual_count = bus.TargetCommandHandShake(buf);
        if (actual_count <= 0) {
            if (!actual_count) {
                LogDebug("Received an unknown command: ${:02x}", buf[0]);
                RaiseDeferredError(ILLEGAL_REQUEST, INVALID_COMMAND_OPERATION_CODE);
            }
            else {
                bus.Reset();
                RaiseDeferredError(ABORTED_COMMAND, COMMAND_PHASE_ERROR);
            }
            return;
        }

        const int command_bytes_count = CommandMetaData::GetInstance().GetByteCount(static_cast<ScsiCommand>(buf[0]));
        assert(command_bytes_count && command_bytes_count <= static_cast<int>(GetCdb().size()));

        for (int i = 0; i < command_bytes_count; ++i) {
            SetCdbByte(i, buf[i]);
        }

        if (script_generator && !script_generator->AddCdb(GetTargetId(), GetEffectiveLun(), GetCdb())) {
            LogWarn("Couldn't append to script file");
        }

        // Check the log level in order to avoid an unnecessary time-consuming string construction
        if (GetLogger().should_log(level::debug)) {
            LogDebug(fmt::runtime(
                CommandMetaData::GetInstance().LogCdb(span(buf.data(), command_bytes_count), "Controller")));
        }

        if (actual_count != command_bytes_count) {
            LogWarn("Received {} byte(s) in COMMAND phase for command ${:02x}, {} required", actual_count, GetCdb()[0],
                command_bytes_count);
            bus.Reset();
            RaiseDeferredError(ABORTED_COMMAND, COMMAND_PHASE_ERROR);
            return;
        }

        const auto control = GetCdb()[command_bytes_count - 1];
        linked = (byte { control } & byte { 0x01 }) != byte { 0 };
        flag = (byte { control } & byte { 0x02 }) != byte { 0 };

        if (flag && !linked) {
            RaiseDeferredError(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
            return;
        }

        // Ensure correct sense data if the previous command was rejected by the controller and not by the device
        if (deferred_sense_key != NO_SENSE
            && static_cast<ScsiCommand>(GetCdb()[0]) == ScsiCommand::REQUEST_SENSE) {
            ProvideSenseData();
            return;
        }
        deferred_sense_key = NO_SENSE;
        deferred_asc = NO_ADDITIONAL_SENSE_INFORMATION;

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
            Error(ILLEGAL_REQUEST, LOGICAL_UNIT_NOT_SUPPORTED);
            return;
        }

        device = GetDeviceForLun(0);
        assert(device);
    }

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (opcode != ScsiCommand::REQUEST_SENSE) {
        SetStatus(GOOD);
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
    else {
        Error(ILLEGAL_REQUEST, NO_ADDITIONAL_SENSE_INFORMATION, RESERVATION_CONFLICT);
    }
}

void Controller::Status()
{
    if (IsStatus()) {
        Send();
        return;
    }

    SetPhase(BusPhase::STATUS,
        fmt::format("STATUS phase, status is {} (status code ${:02x})", STATUS_MAPPING.at(GetStatus()),
            to_underlying(GetStatus())));

    bus.SetMSG(false);
    bus.SetCD(true);
    bus.SetIO(true);

    ResetOffset();
    SetCurrentLength(1);
    SetTransferSize(1, 1);

    // If this is a successfully terminated linked command convert the status code
    GetBuffer()[0] =
        linked && GetStatus() == GOOD ?
            to_underlying(INTERMEDIATE) : to_underlying(GetStatus());
}

void Controller::MsgIn()
{
    if (IsMsgIn()) {
        Send();
        return;
    }

    SetPhase(BusPhase::MSG_IN, "MESSAGE IN phase");

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

    SetPhase(BusPhase::MSG_OUT, "MESSAGE OUT phase");

    // Process the IDENTIFY message
    if (IsSelection()) {
        atn_msg = true;
        msg_bytes.clear();
    }

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

    SetPhase(BusPhase::DATA_IN, "DATA IN phase");

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

    SetPhase(BusPhase::DATA_OUT, "DATA OUT phase");

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
    if (asc == LOGICAL_UNIT_NOT_SUPPORTED || !GetDeviceForLun(lun)) {
        assert(GetDeviceForLun(0));
        lun = 0;
    }

    if (sense_key != NO_SENSE || asc != NO_ADDITIONAL_SENSE_INFORMATION) {
        LogDebug(fmt::runtime(FormatSenseData(sense_key, asc)));

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
        assert(static_cast<size_t>(GetOffset() + length) <= GetBuffer().size());
        const span<uint8_t> data(GetBuffer().data() + GetOffset(), length);

        if (IsDataIn() && GetLogger().should_log(level::trace)) {
            const string &bytes = FormatBytes(data, length);
            LogTrace("Sending {} byte(s) at offset {} in DATA IN phase{}{}", length, GetOffset(),
                bytes.empty() ? "" : ":\n", bytes);
        }

        // The DaynaPort delay work-around for the Mac should be taken from the respective LUN, but as there are
        // no Mac Daynaport drivers for LUNs other than 0 the current work-around is fine. The work-around is
        // required for cases where the actually requested LUN does not exist but is tested for with INQUIRY.
        if (const int l = bus.TargetSendHandShake(data, GetDeviceForLun(0)->GetDelayAfterBytes()); l != length) {
            LogWarn("Sent {} byte(s), {} required", l, length);
            bus.Reset();
            Error(ABORTED_COMMAND, DATA_PHASE_ERROR);
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

    // All data has been transferred

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
            GetBuffer()[0] = to_underlying(
                flag ? MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG : MessageCode::LINKED_COMMAND_COMPLETE);
        }
        else {
            GetBuffer()[0] = to_underlying(MessageCode::COMMAND_COMPLETE);
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
        assert(static_cast<size_t>(GetOffset() + curr_length) <= GetBuffer().size());
        const span<uint8_t> data(GetBuffer().data() + GetOffset(), curr_length);

        if (!IsMsgOut()) {
            LogTrace("Receiving {} byte(s) at offset {}", curr_length, GetOffset());
        }

        if (const int l = bus.TargetReceiveHandShake(data); l != curr_length) {
            LogWarn("Received {} byte(s), {} required", l, curr_length);
            bus.Reset();
            Error(ABORTED_COMMAND, DATA_PHASE_ERROR);
            return;
        }

        if (IsDataOut() && GetLogger().should_log(level::trace)) {
            const string &bytes = FormatBytes(data, curr_length);
            LogTrace("Received {} byte(s) in DATA OUT phase{}{}", curr_length, bytes.empty() ? "" : ":\n", bytes);
        }

        if (IsDataOut() && script_generator && !script_generator->AddData(data)) {
            LogWarn("Couldn't append to script file");
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
        // All data has been transferred
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
    const auto meta_data = CommandMetaData::GetInstance().GetCdbMetaData(static_cast<ScsiCommand>(GetCdb()[0]));
    assert(meta_data.has_data_out);

    int transferred_length = length;
    const auto device = GetDeviceForLun(GetEffectiveLun());
    try {
        if (meta_data.has_custom_data_out && device->GetType() != SCSG) {
            // The offset is the number of bytes transferred, i.e. the length of the parameter list
            device->ModeSelect(GetCdb(), GetBuffer(), GetOffset());
        }
        else {
            transferred_length = device->WriteData(GetCdb(), GetBuffer(), length);
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
            LogTrace("Received message byte ${:02x}", msg);
        }
    }
}

void Controller::ParseMessage()
{
    int rejects = 0;

    for (size_t i = 0; i < msg_bytes.size(); ++i) {
        const uint8_t msg_byte = msg_bytes[i];

        if (msg_byte >= 0x80) {
            identified_lun = static_cast<int>(msg_byte) & 0x1f;
            LogTrace("Received IDENTIFY message for LUN {}", identified_lun);
        }
        else if (msg_byte == 0x01) {
            LogExtendedMessage(i);
            ++rejects;

            // Skip the length byte and the 'length' bytes that follow (a length of 0 means 256)
            if (i + 1 < msg_bytes.size()) {
                i += 1 + (msg_bytes[i + 1] ? msg_bytes[i + 1] : 256);
            }
        }
        else if (msg_byte >= 0x20 && msg_byte <= 0x2f) {
            // Two-byte messages are not supported
            LogTrace("Rejecting two-byte message ${:02x}", msg_byte);
            ++rejects;
            ++i;
        }
        else {
            switch (msg_byte) {
            case to_underlying(MessageCode::ABORT):
                LogTrace("Received ABORT message");
                BusFree();
                return;

            case to_underlying(MessageCode::BUS_DEVICE_RESET):
                LogTrace("Received BUS DEVICE RESET message");
                BusDeviceReset();
                return;

            case 0x05:
                // INITIATOR DETECTED ERROR
            case 0x07:
                // MESSAGE REJECT, never answer with MESSAGE REJECT
            case 0x08:
                // NO OPERATION
            case 0x09:
                // MESSAGE PARITY ERROR
                break;

            default:
                LogTrace("Rejecting unsupported message ${:02x}", msg_byte);
                ++rejects;
                break;
            }
        }
    }

    // One MESSAGE REJECT byte per rejected message, all in a single MESSAGE IN phase
    if (rejects) {
        SetCurrentLength(rejects);
        SetTransferSize(rejects, rejects);
        fill_n(GetBuffer().begin(), rejects, 0x07);
        MsgIn();
    }
}

void Controller::LogExtendedMessage(size_t start) const
{
    // msg_bytes[start] is the extended message marker 01h, followed by the length byte and the
    // extended message code. The length counts the code and the arguments, a length of 0 means 256.
    if (start + 1 >= msg_bytes.size()) {
        LogWarn("Truncated extended message");
        return;
    }

    if (const size_t length = msg_bytes[start + 1] ? msg_bytes[start + 1] : 256; start + 2 + length
        > msg_bytes.size()) {
        LogWarn("Truncated extended message");
        return;
    }

    switch (msg_bytes[start + 2]) {
    case 0x00:
        LogTrace("Rejecting MODIFY DATA POINTER message");
        break;

    case 0x01:
        LogTrace("Rejecting SYNCHRONOUS DATA TRANSFER REQUEST message");
        break;

    case 0x03:
        LogTrace("Rejecting WIDE DATA TRANSFER REQUEST message");
        break;

    case 0x04:
        LogTrace("Rejecting PARALLEL PROTOCOL REQUEST message");
        break;

    case 0x05:
        LogTrace("Rejecting MODIFY BIDIRECTIONAL DATA POINTER message");
        break;

    default:
        LogTrace("Rejecting extended message ${:02x}", msg_bytes[start + 2]);
        break;
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
        msg_bytes.clear();

        if (IsBusFree()) {
            return;
        }

        if (IsMsgIn()) {
            atn_msg = true;
            return;
        }
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

void Controller::BusDeviceReset()
{
    deferred_sense_key = NO_SENSE;
    deferred_asc = NO_ADDITIONAL_SENSE_INFORMATION;

    // BUS DEVICE RESET applies to all LUNs
    for (const auto &device : GetDevices()) {
        device->SetReset(true);
        device->DiscardReservation();
        device->ResetStatus();
    }

    BusFree();
}

void Controller::RaiseDeferredError(SenseKey s, Asc a)
{
    deferred_sense_key = s;
    deferred_asc = a;
    Error(s, a);
}

void Controller::ProvideSenseData()
{
    SetCurrentLength(min(18, static_cast<int>(GetCdb()[4])));

    auto &buf = GetBuffer();
    fill_n(buf.begin(), 18, 0);
    buf[0] = 0x70;
    buf[2] = to_underlying(deferred_sense_key);
    buf[7] = 10;
    buf[12] = to_underlying(deferred_asc);

    deferred_sense_key = NO_SENSE;
    deferred_asc = NO_ADDITIONAL_SENSE_INFORMATION;

    DataIn();
}

int Controller::GetEffectiveLun() const
{
    // Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
    return identified_lun != -1 ? identified_lun : to_underlying(byte { GetCdb()[1] } >> 5);
}
