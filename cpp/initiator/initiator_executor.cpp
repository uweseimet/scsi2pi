//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "initiator_executor.h"
#include <chrono>
#include "initiator_util.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_util.h"

using namespace chrono;
using namespace s2p_util;
using namespace initiator_util;

int InitiatorExecutor::Execute(ScsiCommand cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length, int timeout,
    bool enable_log)
{
    cdb[0] = static_cast<uint8_t>(cmd);
    return Execute(cdb, buffer, length, timeout, enable_log);
}

int InitiatorExecutor::Execute(span<uint8_t> cdb, span<uint8_t> buffer, int length, int timeout, bool enable_log)
{
    bus.Reset();

    status_code = 0xff;
    byte_count = 0;
    cdb_offset = 0;

    const auto cmd = static_cast<ScsiCommand>(cdb[0]);

    auto command_name = string(CommandMetaData::GetInstance().GetCommandName(cmd));
    if (command_name.empty()) {
        command_name = fmt::format("${:02x}", static_cast<int>(cmd));
    }

    // Only report byte count mismatch for non-linked commands
    if (const int count = CommandMetaData::GetInstance().GetByteCount(cmd); count
        && count != static_cast<int>(cdb.size()) && !(static_cast<int>(cdb[cdb_offset + 5]) & 0x01)) {
        initiator_logger.warn("CDB has {0} byte(s), command {1} requires {2} bytes", cdb.size(), command_name, count);
    }

    initiator_logger.debug(CommandMetaData::GetInstance().LogCdb(cdb, "Initiator"));

    // There is no arbitration phase with SASI
    if (!sasi && !Arbitration()) {
        bus.Reset();
        return 0xff;
    }

    if (!Selection(static_cast<int>(cdb[1]) & 0b11100000)) {
        bus.Reset();
        return 0xff;
    }

    // Wait for the command to finish
    auto now = steady_clock::now();
    while ((duration_cast<seconds>(steady_clock::now() - now).count()) < timeout) {
        bus.Acquire();

        if (bus.GetREQ()) {
            try {
                if (Dispatch(cdb, buffer, length)) {
                    now = steady_clock::now();
                }
                else if (static_cast<StatusCode>(status_code) != StatusCode::INTERMEDIATE) {
                    break;
                }
            }
            catch (const PhaseException &e) {
                initiator_logger.error(e.what());
                ResetBus(bus);
                return 0xff;
            }
        }
    }

    if ((duration_cast<seconds>(steady_clock::now() - now).count()) >= timeout) {
        initiator_logger.error("Timeout");
    }

    if (enable_log) {
        initiator_logger.warn(GetStatusString(status_code));
    }

    return status_code;
}

bool InitiatorExecutor::Dispatch(span<uint8_t> cdb, span<uint8_t> buffer, int &length)
{
    const BusPhase phase = bus.GetPhase();

    initiator_logger.trace("Current phase is {}", Bus::GetPhaseName(phase));

    switch (phase) {
    case BusPhase::COMMAND:
        Command(cdb);
        break;

    case BusPhase::STATUS:
        Status();
        break;

    case BusPhase::DATA_IN:
        DataIn(buffer, length);
        break;

    case BusPhase::DATA_OUT:
        DataOut(buffer, length);
        break;

    case BusPhase::MSG_IN:
        MsgIn();
        if (next_message == MessageCode::IDENTIFY) {
            // Done with this command cycle unless there is a pending MESSAGE REJECT
            return false;
        }
        break;

    case BusPhase::MSG_OUT:
        MsgOut();
        break;

    default:
        initiator_logger.warn("Ignoring {} phase", Bus::GetPhaseName(phase));
        return false;
    }

    return true;
}

bool InitiatorExecutor::Arbitration() const
{
    initiator_logger.trace("Arbitration with initiator ID {}", initiator_id);

    if (!WaitForFree()) {
        initiator_logger.trace("Bus is not free");
        return false;
    }

    Sleep(BUS_FREE_DELAY);

    bus.SetDAT(static_cast<uint8_t>(1 << initiator_id));

    bus.SetBSY(true);

    Sleep(ARBITRATION_DELAY);

    if (bus.GetDAT() > (1 << initiator_id)) {
        initiator_logger.trace("Lost arbitration, winning initiator ID is {}", bus.GetDAT() - (1 << initiator_id));
        return false;
    }

    bus.SetSEL(true);

    Sleep(BUS_CLEAR_DELAY);
    Sleep(BUS_SETTLE_DELAY);

    return true;
}

bool InitiatorExecutor::Selection(bool explicit_lun) const
{
    initiator_logger.trace("Selection of target {0} with initiator ID {1}", target_id, initiator_id);

    // There is no initiator ID with SASI
    bus.SetDAT(static_cast<uint8_t>((sasi ? 0 : 1 << initiator_id) + (1 << target_id)));

    bus.SetSEL(true);

    if (!sasi && !explicit_lun) {
        // Request MESSAGE OUT for IDENTIFY
        bus.SetATN(true);

        Sleep(DESKEW_DELAY);
        Sleep(DESKEW_DELAY);
    }

    bus.SetBSY(false);

    Sleep(BUS_SETTLE_DELAY);

    if (!WaitForBusy()) {
        initiator_logger.trace("Selection failed");
        return false;
    }

    Sleep(DESKEW_DELAY);
    Sleep(DESKEW_DELAY);

    bus.SetSEL(false);

    return true;
}

void InitiatorExecutor::Command(span<uint8_t> cdb)
{
    if (target_lun < 8) {
        // Encode LUN in the CDB for backwards compatibility with SCSI-1-CCS
        cdb[cdb_offset + 1] = static_cast<uint8_t>(cdb[1] + (target_lun << 5));
    }

    const auto cmd = static_cast<ScsiCommand>(cdb[cdb_offset]);
    const int sent_count = bus.SendHandShake(cdb.data() + cdb_offset, static_cast<int>(cdb.size()) - cdb_offset);
    if (static_cast<int>(cdb.size()) < sent_count) {
        initiator_logger.error("Execution of {} failed", CommandMetaData::GetInstance().GetCommandName(cmd));
    }

    cdb_offset += sent_count;
}

void InitiatorExecutor::Status()
{
    array<uint8_t, 1> buf = { };

    if (bus.ReceiveHandShake(buf.data(), 1) != 1) {
        initiator_logger.error("STATUS phase failed");
    }
    else {
        status_code = buf[0];
    }
}

void InitiatorExecutor::DataIn(data_in_t buf, int &length)
{
    if (!length) {
        throw PhaseException("Buffer full in DATA IN phase");
    }

    initiator_logger.trace("Receiving up to {0} byte(s) in DATA IN phase", length);

    byte_count = bus.ReceiveHandShake(buf.data(), length);

    length -= byte_count;
}

void InitiatorExecutor::DataOut(data_out_t buf, int &length)
{
    if (!length) {
        throw PhaseException("No more data for DATA OUT phase");
    }

    initiator_logger.debug("Sending {0} byte(s):\n{1}", length, formatter.FormatBytes(buf, length));

    byte_count = bus.SendHandShake(buf.data(), length);
    if (byte_count != length) {
        initiator_logger.error("Initiator sent {0} byte(s) in DATA OUT phase, expected size was {1} byte(s)", byte_count, length);
        throw PhaseException("DATA OUT phase failed");
    }

    length -= byte_count;
}

void InitiatorExecutor::MsgIn()
{
    const int msg = bus.MsgInHandShake();
    switch (msg) {
    case -1:
        initiator_logger.error("MESSAGE IN phase failed");
        break;

    case static_cast<int>(MessageCode::COMMAND_COMPLETE):
        initiator_logger.trace("Received COMMAND COMPLETE");
        break;

    case static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE):
        initiator_logger.trace("Received LINKED COMMAND COMPLETE");
        break;

    case static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG):
        initiator_logger.trace("Received LINKED COMMAND COMPLETE WITH FLAG");
        break;

    default:
        initiator_logger.trace("Device did not report command completion, rejecting unsupported message ${:02x}", msg);
        next_message = MessageCode::MESSAGE_REJECT;
        break;
    }
}

void InitiatorExecutor::MsgOut()
{
    array<uint8_t, 1> buf;

    // IDENTIFY or MESSAGE REJECT
    buf[0] = static_cast<uint8_t>(target_lun) + static_cast<uint8_t>(next_message);

    if (bus.SendHandShake(buf.data(), buf.size()) != buf.size()) {
        initiator_logger.error("MESSAGE OUT phase for {} message failed",
            next_message == MessageCode::IDENTIFY ? "IDENTIFY" : "MESSAGE REJECT");
    }

    // Reset default message for MESSAGE OUT to IDENTIFY
    next_message = MessageCode::IDENTIFY;
}

bool InitiatorExecutor::WaitForFree() const
{
    // Wait for up to 2 s
    int count = 10'000;
    do {
        // Wait 20 ms
        Sleep( { .tv_sec = 0, .tv_nsec = 20'000 });
        bus.Acquire();
        if (!bus.GetBSY() && !bus.GetSEL()) {
            return true;
        }
    } while (count--);

    return false;
}

bool InitiatorExecutor::WaitForBusy() const
{
    // Wait for up to 2 s
    int count = 10'000;
    do {
        // Wait 20 ms
        Sleep( { .tv_sec = 0, .tv_nsec = 20'000 });
        bus.Acquire();
        if (bus.GetBSY()) {
            return true;
        }
    } while (count--);

    return false;
}

void InitiatorExecutor::SetTarget(int id, int lun, bool s)
{
    target_id = id;
    target_lun = lun;
    sasi = s;
}

