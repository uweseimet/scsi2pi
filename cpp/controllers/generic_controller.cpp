//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "buses/gpio_bus.h"
#include "devices/disk.h"
#ifdef BUILD_SCDP
#include "devices/daynaport.h"
#endif
#include "generic_controller.h"

using namespace scsi_defs;

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
        Abort(asc::controller_process_phase);
        return false;
    }

    return !IsBusFree();
}

void GenericController::BusFree()
{
    if (!IsBusFree()) {
        LogTrace("Bus Free phase");
        SetPhase(phase_t::busfree);

        GetBus().SetREQ(false);
        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(false);
        GetBus().SetBSY(false);

        // Initialize status and message
        SetStatus(status::good);
        SetMessage(0x00);

        SetByteTransfer(false);

        return;
    }

    if (GetBus().GetSEL() && !GetBus().GetBSY()) {
        Selection();
    }
}

void GenericController::Selection()
{
    if (!IsSelection()) {
        LogTrace("Selection phase");
        SetPhase(phase_t::selection);

        GetBus().SetBSY(true);
        return;
    }

    if (!GetBus().GetSEL() && GetBus().GetBSY()) {
        LogTrace("Selection completed");

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
        LogTrace("Command phase");
        SetPhase(phase_t::command);

        GetBus().SetMSG(false);
        GetBus().SetCD(true);
        GetBus().SetIO(false);

        const int actual_count = GetBus().CommandHandShake(GetBuffer());
        if (!actual_count) {
            LogTrace(fmt::format("Received unknown command: ${:02x}", static_cast<int>(GetBuffer()[0])));

            Error(sense_key::illegal_request, asc::invalid_command_operation_code);
            return;
        }

        const int command_byte_count = Bus::GetCommandByteCount(GetBuffer()[0]);

        AllocateCmd(command_byte_count);
        for (int i = 0; i < command_byte_count; i++) {
            SetCmdByte(i, GetBuffer()[i]);
        }

        if (spdlog::get_level() <= spdlog::level::debug) {
            string s = fmt::format("Controller is executing {}, CDB $",
                command_mapping.find(GetOpcode())->second.second);
            for (int i = 0; i < Bus::GetCommandByteCount(static_cast<uint8_t>(GetOpcode())); i++) {
                s += fmt::format("{:02x}", GetCmdByte(i));
            }
            LogDebug(s);
        }

        if (actual_count != command_byte_count) {
            LogError(fmt::format(
                "Command byte count mismatch for command ${0:02x}: expected {1} bytes, received {2} byte(s)",
                static_cast<int>(GetOpcode()), command_byte_count, actual_count));
            Error(sense_key::illegal_request, asc::invalid_field_in_cdb);
            return;
        }

        SetLength(0);

        Execute();
    }
}

void GenericController::Execute()
{
    // Initialization for data transfer
    ResetOffset();
    SetBlocks(1);

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (GetOpcode() != scsi_command::cmd_request_sense) {
        SetStatus(status::good);
    }

    int lun = GetEffectiveLun();
    if (!HasDeviceForLun(lun)) {
        if (GetOpcode() != scsi_command::cmd_inquiry && GetOpcode() != scsi_command::cmd_request_sense) {
            LogTrace(fmt::format("Invalid LUN {}", lun));

            Error(sense_key::illegal_request, asc::invalid_lun);

            return;
        }

        assert(HasDeviceForLun(0));

        lun = 0;
    }

    // SCSI-2 4.4.3 Incorrect logical unit handling
    if (GetOpcode() == scsi_command::cmd_inquiry && !HasDeviceForLun(lun)) {
        LogTrace(fmt::format("Reporting LUN {} as not supported", GetEffectiveLun()));

        GetBuffer().data()[0] = 0x7f;

        return;
    }

    auto device = GetDeviceForLun(lun);

    // Discard pending sense data from the previous command if the current command is not REQUEST SENSE
    if (GetOpcode() != scsi_command::cmd_request_sense) {
        device->SetStatusCode(0);
    }

    if (device->CheckReservation(initiator_id, GetOpcode(), GetCmdByte(4) & 0x01)) {
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
        LogTrace(fmt::format("Status phase, status is ${:02x}", static_cast<int>(GetStatus())));
        SetPhase(phase_t::status);

        // Signal line operated by the target
        GetBus().SetMSG(false);
        GetBus().SetCD(true);
        GetBus().SetIO(true);

        // Data transfer is 1 byte x 1 block
        ResetOffset();
        SetLength(1);
        SetBlocks(1);
        GetBuffer()[0] = (uint8_t)GetStatus();

        return;
    }

    Send();
}

void GenericController::MsgIn()
{
    if (!IsMsgIn()) {
        LogTrace("Message In phase");
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
        if (!HasValidLength()) {
            Status();
            return;
        }

        LogTrace("Data In phase");
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
        if (!HasValidLength()) {
            Status();
            return;
        }

        LogTrace("Data Out phase");
        SetPhase(phase_t::dataout);

        GetBus().SetMSG(false);
        GetBus().SetCD(false);
        GetBus().SetIO(false);

        ResetOffset();
        return;
    }

    Receive();
}

void GenericController::Error(sense_key sense_key, asc asc, status status)
{
    GetBus().Acquire();

    if (GetBus().GetRST()) {
        BusFree();
        return;
    }

    // Bus free for status phase and message in phase
    if (IsStatus() || IsMsgIn()) {
        BusFree();
        return;
    }

    int lun = GetEffectiveLun();
    if (!HasDeviceForLun(lun) || asc == asc::invalid_lun) {
        lun = 0;
    }

    if (sense_key != sense_key::no_sense || asc != asc::no_additional_sense_information) {
        LogDebug(
            fmt::format("Error status: Sense Key ${0:02x}, ASC ${1:02x}", static_cast<int>(sense_key),
                static_cast<int>(asc)));

        // Set Sense Key and ASC for a subsequent REQUEST SENSE
        GetDeviceForLun(lun)->SetStatusCode((static_cast<int>(sense_key) << 16) | (static_cast<int>(asc) << 8));
    }

    SetStatus(status);
    SetMessage(0x00);

    Status();
}

void GenericController::Abort(asc asc)
{
    // The vendor-specific ASC and ASCQ help to locate where ABORTED_COMMAND has been raised
    Error(sense_key::aborted_command, asc);
}

void GenericController::Send()
{
    assert(!GetBus().GetREQ());
    assert(GetBus().GetIO());

    if (HasValidLength()) {
        LogTrace(fmt::format("Sending data, offset: {0}, length: {1}", GetOffset(), GetLength()));

        assert(HasDeviceForLun(0));

        // The delay should be taken from the respective LUN, but as there are no Mac Daynaport drivers
        // for LUNs other than 0 this work-around works.
        if (const int len = GetBus().SendHandShake(GetBuffer().data() + GetOffset(), GetLength(),
            GetDeviceForLun(0)->GetDelayAfterBytes()); len != static_cast<int>(GetLength())) {
            Abort(asc::controller_send_handshake);
        }
        else {
            UpdateOffsetAndLength();
        }

        return;
    }

    DecrementBlocks();

    // Processing after data collection (read/data-in only)
    if (IsDataIn() && HasBlocks()) {
        // Set next buffer (set offset, length)
        if (!XferIn(GetBuffer())) {
            Abort(asc::controller_send_xfer_in);
            return;
        }

        LogTrace("Processing after data collection");
    }

    // Continue sending if blocks != 0
    if (HasBlocks()) {
        LogTrace("Continuing to send");
        assert(HasValidLength());
        assert(GetOffset() == 0);
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
        SetLength(1);
        SetBlocks(1);
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

    if (HasValidLength()) {
        LogTrace(fmt::format("Receiving {} byte(s)", GetLength()));

        // If not able to receive all, move to status phase
        if (uint32_t len = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), GetLength()); len
            != GetLength()) {
            LogError(fmt::format("Not able to receive {0} byte(s), only received {1}", GetLength(), len));
            Abort(asc::controller_receive_handshake);
            return;
        }
    }

    if (IsByteTransfer()) {
        ReceiveBytes();
        return;
    }

    if (HasValidLength()) {
        UpdateOffsetAndLength();
        return;
    }

    DecrementBlocks();
    bool result = true;

    // Processing after receiving data (by phase)
    switch (GetPhase()) {
    case phase_t::dataout:
        if (!HasBlocks()) {
            // End with this buffer
            result = XferOut(false);
        } else {
            // Continue to next buffer (set offset, length)
            result = XferOut(true);
        }
        break;

    case phase_t::msgout:
        SetMessage(GetBuffer()[0]);

        XferMsg(GetMessage());

        // Clear message data in preparation for Message In
        SetMessage(0x00);
        break;

    default:
        break;
    }

    if (!result) {
        Abort(asc::controller_receive_result);
        return;
    }

    // Continue to receive if blocks != 0
    if (HasBlocks()) {
        assert(HasValidLength());
        assert(GetOffset() == 0);
        return;
    }

    // Move to next phase
    switch (GetPhase()) {
    case phase_t::command:
        ProcessCommand();
        break;

    case phase_t::msgout:
        ProcessMessage();
        break;

    case phase_t::dataout:
        // Block-oriented data have been handled above
        DataOutNonBlockOriented();

        Status();
        break;

    default:
        assert(false);
        break;
    }
}

void GenericController::ReceiveBytes()
{
    if (HasValidLength()) {
        InitBytesToTransfer();
        UpdateOffsetAndLength();
        return;
    }

    bool result = true;

    // Processing after receiving data
    switch (GetPhase()) {
    case phase_t::dataout:
        result = XferOut(false);
        break;

    case phase_t::msgout:
        SetMessage(GetBuffer()[0]);

        XferMsg(GetMessage());

        // Clear message data in preparation for Message In
        SetMessage(0x00);
        break;

    default:
        break;
    }

    if (!result) {
        Abort(asc::controller_receive_bytes_result);
        return;
    }

    // Move to next phase
    switch (GetPhase()) {
    case phase_t::command:
        ProcessCommand();
        break;

    case phase_t::msgout:
        ProcessMessage();
        break;

    case phase_t::dataout:
        Status();
        break;

    default:
        assert(false);
        break;
    }
}

bool GenericController::XferOut(bool cont)
{
    assert(IsDataOut());

    if (!IsByteTransfer()) {
        return XferOutBlockOriented(cont);
    }

    const uint32_t count = GetBytesToTransfer();
    SetByteTransfer(false);

    auto device = GetDeviceForLun(GetEffectiveLun());
    return device ? device->WriteByteSequence(span(GetBuffer().data(), count)) : false;
}

void GenericController::DataOutNonBlockOriented() const
{
    assert(IsDataOut());

    switch (GetOpcode()) {
    case scsi_command::cmd_write6:
        case scsi_command::cmd_write10:
        case scsi_command::cmd_write16:
        case scsi_command::cmd_write_long10:
        case scsi_command::cmd_write_long16:
        case scsi_command::cmd_verify10:
        case scsi_command::cmd_verify16:
        case scsi_command::cmd_mode_select6:
        case scsi_command::cmd_mode_select10:
        break;

    case scsi_command::cmd_set_mcast_addr:
        // TODO: Eventually, we should store off the multicast address configuration data here
        break;

    case scsi_command::cmd_set_iface_mode:
        // TODO Should the DaynaPort MAC address actually be set here?
        break;

    default:
        LogWarn(fmt::format("Unexpected Data Out phase for command ${:02x}", static_cast<int>(GetOpcode())));
        break;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool GenericController::XferIn(vector<uint8_t> &buf)
{
    assert(IsDataIn());

    LogTrace(fmt::format("Command: ${:02x}", static_cast<int>(GetOpcode())));

    const int lun = GetEffectiveLun();
    if (!HasDeviceForLun(lun)) {
        return false;
    }

    // Limited to read commands
    switch (GetOpcode()) {
    case scsi_command::cmd_read6:
        case scsi_command::cmd_read10:
        case scsi_command::cmd_read16:
        #ifdef BUILD_DISK
        try {
            SetLength(dynamic_pointer_cast<Disk>(GetDeviceForLun(lun))->Read(buf, GetNext()));
        }
        catch (const scsi_exception&) {
            // If there is an error, go to the status phase
            return false;
        }

        IncrementNext();

        // If things are normal, work setting
        ResetOffset();
#endif
        break;

    default:
        assert(false);
        return false;
    }

    return true;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool GenericController::XferOutBlockOriented(bool cont)
{
    auto device = GetDeviceForLun(GetEffectiveLun());

    // Limited to write commands
    switch (GetOpcode()) {
    case scsi_command::cmd_mode_select6:
        case scsi_command::cmd_mode_select10:
        {
#ifdef BUILD_MODE_PAGE_DEVICE
        auto mode_page_device = dynamic_pointer_cast<ModePageDevice>(device);
        if (!mode_page_device) {
            return false;
        }

        try {
            mode_page_device->ModeSelect(GetOpcode(), GetCmd(), GetBuffer(), GetOffset());
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());
            return false;
        }
#endif
        break;
    }

    case scsi_command::cmd_write6:
        case scsi_command::cmd_write10:
        case scsi_command::cmd_write16:
        {
#ifdef BUILD_SCDP
        // TODO Get rid of this special case for SCDP
        if (auto daynaport = dynamic_pointer_cast<DaynaPort>(device); daynaport) {
            if (!daynaport->Write(GetCmd(), GetBuffer())) {
                return false;
            }

            ResetOffset();
            break;
        }
#endif

#ifdef BUILD_DISK
        auto disk = dynamic_pointer_cast<Disk>(device);
        if (!disk) {
            return false;
        }

        try {
            disk->Write(GetBuffer(), GetNext() - 1);
        }
        catch (const scsi_exception &e) {
            Error(e.get_sense_key(), e.get_asc());

            return false;
        }

        // If you do not need the next block, end here
        IncrementNext();
        if (cont) {
            SetLength(disk->GetSectorSizeInBytes());
            ResetOffset();
        }
#endif
        break;
    }

    case scsi_command::cmd_verify10:
        case scsi_command::cmd_verify16:
        {
#ifdef BUILD_DISK
        auto disk = dynamic_pointer_cast<Disk>(device);
        if (!disk) {
            return false;
        }

        // If you do not need the next block, end here
        IncrementNext();
        if (cont) {
            SetLength(disk->GetSectorSizeInBytes());
            ResetOffset();
        }
#endif
        break;
    }

    case scsi_command::cmd_set_mcast_addr:
        LogTrace("Ignored DaynaPort Set Multicast Address");
        break;

    case scsi_command::cmd_set_iface_mode:
        LogTrace("Ignored DaynaPort Set Interface Mode");
        break;

    default:
        LogWarn(fmt::format("Received unexpected command ${:02x}", static_cast<int>(GetOpcode())));
        break;
    }

    return true;
}
#pragma GCC diagnostic pop

void GenericController::ProcessCommand()
{
    const uint32_t len = GpioBus::GetCommandByteCount(GetBuffer()[0]);

    string s = "CDB=$";
    for (uint32_t i = 0; i < len; i++) {
        SetCmdByte(i, GetBuffer()[i]);
        s += fmt::format("{:02x}", GetCmdByte(i));
    }
    LogTrace(s);

    Execute();
}
