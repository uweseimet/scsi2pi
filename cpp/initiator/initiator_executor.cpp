//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "initiator_executor.h"
#include <chrono>
#include "initiator_util.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_util.h"

using namespace s2p_util;
using namespace initiator_util;

int InitiatorExecutor::Execute(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length, int timeout)
{
    cdb[0] = static_cast<uint8_t>(cmd);
    return Execute(cdb, buffer, length, timeout);
}

int InitiatorExecutor::Execute(span<uint8_t> cdb, span<uint8_t> buffer, int length, int timeout)
{
    bus.Reset();

    status = 0xff;
    byte_count = 0;
    cdb_offset = 0;

    const auto cmd = static_cast<scsi_command>(cdb[0]);

    auto command_name = string(CommandMetaData::Instance().GetCommandName(cmd));
    if (command_name.empty()) {
        command_name = fmt::format("${:02x}", static_cast<int>(cmd));
    }

    // Only report byte count mismatch for non-linked commands
    if (const int count = CommandMetaData::Instance().GetCommandBytesCount(cmd); count
        && count != static_cast<int>(cdb.size()) && !(static_cast<int>(cdb[cdb_offset + 5]) & 0x01)) {
        initiator_logger.warn("CDB has {0} byte(s), command {1} requires {2} bytes", cdb.size(), command_name, count);
    }

    initiator_logger.debug(CommandMetaData::Instance().LogCdb(cdb, "Initiator"));

    // There is no arbitration phase with SASI
    if (!sasi && !Arbitration()) {
        bus.Reset();
        return 0xff;
    }

    if (!Selection()) {
        bus.Reset();
        return 0xff;
    }

    // Wait for the command to finish
    auto now = chrono::steady_clock::now();
    while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < timeout) {
        bus.Acquire();

        if (bus.GetREQ()) {
            try {
                if (Dispatch(cdb, buffer, length)) {
                    now = chrono::steady_clock::now();
                    continue;
                }

                if (static_cast<status_code>(status) != status_code::intermediate) {
                    LogStatus();
                    return status;
                }
            }
            catch (const phase_exception &e) {
                initiator_logger.error(e.what());
                ResetBus(bus);
                return 0xff;
            }
        }
    }

    return 0xff;
}

bool InitiatorExecutor::Dispatch(span<uint8_t> cdb, span<uint8_t> buffer, int &length)
{
    const bus_phase phase = bus.GetPhase();

    initiator_logger.trace("Current phase is {}", Bus::GetPhaseName(phase));

    switch (phase) {
    case bus_phase::command:
        Command(cdb);
        break;

    case bus_phase::status:
        Status();
        break;

    case bus_phase::datain:
        DataIn(buffer, length);
        break;

    case bus_phase::dataout:
        DataOut(buffer, length);
        break;

    case bus_phase::msgin:
        MsgIn();
        if (next_message == message_code::identify) {
            // Done with this command cycle unless there is a pending MESSAGE REJECT
            return false;
        }
        break;

    case bus_phase::msgout:
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

bool InitiatorExecutor::Selection() const
{
    initiator_logger.trace("Selection of target {0} with initiator ID {1}", target_id, initiator_id);

    // There is no initiator ID with SASI
    bus.SetDAT(static_cast<uint8_t>((sasi ? 0 : 1 << initiator_id) + (1 << target_id)));

    bus.SetSEL(true);

    if (!sasi) {
        // Request MESSAGE OUT for IDENTIFY
        bus.SetATN(true);

        Sleep(DESKEW_DELAY);
        Sleep(DESKEW_DELAY);

        bus.SetBSY(false);

        Sleep(BUS_SETTLE_DELAY);
    }

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

    const auto cmd = static_cast<scsi_command>(cdb[cdb_offset]);
    const int sent_count = bus.SendHandShake(cdb.data() + cdb_offset, static_cast<int>(cdb.size()) - cdb_offset);
    if (static_cast<int>(cdb.size()) < sent_count) {
        if (const string_view &command_name = CommandMetaData::Instance().GetCommandName(cmd); !command_name.empty()) {
            initiator_logger.error("Command {} failed", command_name);
        }
        else {
            initiator_logger.error("Command ${:02x} failed", static_cast<int>(cmd));
        }
    }

    cdb_offset += sent_count;
}

void InitiatorExecutor::Status()
{
    array<uint8_t, 1> buf = { };

    if (bus.ReceiveHandShake(buf.data(), static_cast<int>(buf.size())) != static_cast<int>(buf.size())) {
        initiator_logger.error("STATUS phase failed");
    }
    else {
        status = buf[0];
    }
}

void InitiatorExecutor::DataIn(data_in_t buf, int &length)
{
    if (!length) {
        throw phase_exception("Buffer full in DATA IN phase");
    }

    initiator_logger.trace("Initiator is receiving up to {0} byte(s) in DATA IN phase", length);

    byte_count = bus.ReceiveHandShake(buf.data(), length);

    length -= byte_count;
}

void InitiatorExecutor::DataOut(data_out_t buf, int &length)
{
    if (!length) {
        throw phase_exception("No more data for DATA OUT phase");
    }

    initiator_logger.debug(fmt::format("Initiator is sending {0} byte(s):\n{1}", length, FormatBytes(buf, length, 128)));

    byte_count = bus.SendHandShake(buf.data(), length);
    if (byte_count != length) {
        initiator_logger.error("Initiator sent {0} byte(s) in DATA OUT phase, expected size was {1} byte(s)", byte_count, length);
        throw phase_exception("DATA OUT phase failed");
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

    case static_cast<int>(message_code::command_complete):
    case static_cast<int>(message_code::linked_command_complete):
    case static_cast<int>(message_code::linked_command_complete_with_flag):
        break;

    default:
        initiator_logger.trace("Device did not report command completion, rejecting unsupported message ${:02x}", msg);
        next_message = message_code::message_reject;
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
            next_message == message_code::identify ? "IDENTIFY" : "MESSAGE REJECT");
    }

    // Reset default message for MESSAGE OUT to IDENTIFY
    next_message = message_code::identify;
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

void InitiatorExecutor::LogStatus() const
{
    if (status) {
        if (const auto &it = STATUS_MAPPING.find(static_cast<status_code>(status)); it != STATUS_MAPPING.end()) {
            initiator_logger.warn("Device reported {0} (status code ${1:02x})", it->second, status);
        }
        else if (status != 0xff) {
            initiator_logger.warn("Device reported an unknown status (status code ${:02x})", status);
        }
        else {
            initiator_logger.warn("Device did not respond");
        }
    }
}

