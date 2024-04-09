//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <chrono>
#include <spdlog/spdlog.h>
#include "shared/s2p_util.h"
#include "buses/bus_factory.h"
#include "initiator_util.h"
#include "initiator_executor.h"

using namespace std;
using namespace spdlog;
using namespace s2p_util;
using namespace initiator_util;

int InitiatorExecutor::Execute(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length, int timeout)
{
    bus.Reset();

    status = 0xff;
    byte_count = 0;

    if (const int count = BusFactory::Instance().GetCommandBytesCount(cmd); count
        && count != static_cast<int>(cdb.size())) {
        warn("CDB has {0} byte(s), command ${1:02x} requires {2} bytes", cdb.size(), static_cast<int>(cmd), count);
    }

    if (const string_view &command_name = BusFactory::Instance().GetCommandName(cmd); !command_name.empty()) {
        trace("Executing command {0} for device {1}:{2}", command_name, target_id, target_lun);
    }
    else {
        trace("Executing command ${0:02x} for device {1}:{2}", static_cast<int>(cmd), target_id, target_lun);
    }

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
                if (Dispatch(cmd, cdb, buffer, length)) {
                    now = chrono::steady_clock::now();
                }
                else {
                    LogStatus();
                    return status;
                }
            }
            catch (const phase_exception &e) {
                error(e.what());
                ResetBus(bus);
                return 0xff;
            }
        }
    }

    return 0xff;
}

bool InitiatorExecutor::Dispatch(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int &length)
{
    const bus_phase phase = bus.GetPhase();

    trace("Current phase is {}", Bus::GetPhaseName(phase));

    switch (phase) {
    case bus_phase::command:
        Command(cmd, cdb);
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
        if (next_message == 0x80) {
            // Done with this command cycle unless there is a pending MESSAGE REJECT
            return false;
        }
        break;

    case bus_phase::msgout:
        MsgOut();
        break;

    default:
        warn("Ignoring {} phase", Bus::GetPhaseName(phase));
        return false;
    }

    return true;
}

bool InitiatorExecutor::Arbitration() const
{
    trace("Arbitration with initiator ID {}", initiator_id);

    if (!WaitForFree()) {
        trace("Bus is not free");
        return false;
    }

    Sleep(BUS_FREE_DELAY);

    bus.SetDAT(static_cast<uint8_t>(1 << initiator_id));

    bus.SetBSY(true);

    Sleep(ARBITRATION_DELAY);

    if (bus.GetDAT() > (1 << initiator_id)) {
        trace("Lost arbitration, winning initiator ID is {}", bus.GetDAT() - (1 << initiator_id));
        return false;
    }

    bus.SetSEL(true);

    Sleep(BUS_CLEAR_DELAY);
    Sleep(BUS_SETTLE_DELAY);

    return true;
}

bool InitiatorExecutor::Selection() const
{
    trace("Selection of target {0} with initiator ID {1}", target_id, initiator_id);

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
        trace("Selection failed");
        return false;
    }

    Sleep(DESKEW_DELAY);
    Sleep(DESKEW_DELAY);

    bus.SetSEL(false);

    return true;
}

void InitiatorExecutor::Command(scsi_command cmd, span<uint8_t> cdb) const
{
    cdb[0] = static_cast<uint8_t>(cmd);
    if (target_lun < 8) {
        // Encode LUN in the CDB for backwards compatibility with SCSI-1-CCS
        cdb[1] = static_cast<uint8_t>(cdb[1] + (target_lun << 5));
    }

    if (static_cast<int>(cdb.size()) != bus.SendHandShake(cdb.data(), static_cast<int>(cdb.size()))) {
        if (const string_view &command_name = BusFactory::Instance().GetCommandName(cmd); !command_name.empty()) {
            error("Command {} failed", command_name);
        }
        else {
            error("Command ${:02x} failed", static_cast<int>(cmd));
        }
    }
}

void InitiatorExecutor::Status()
{
    array<uint8_t, 1> buf;

    if (bus.ReceiveHandShake(buf.data(), static_cast<int>(buf.size())) != static_cast<int>(buf.size())) {
        error("STATUS phase failed");
    }
    else {
        status = buf[0];
    }
}

void InitiatorExecutor::DataIn(span<uint8_t> buffer, int &length)
{
    if (!length) {
        throw phase_exception("Buffer full in DATA IN phase");
    }

    trace("Receiving up to {0} byte(s) in DATA IN phase", length);

    byte_count = bus.ReceiveHandShake(buffer.data(), length);

    length -= byte_count;
}

void InitiatorExecutor::DataOut(span<uint8_t> buffer, int &length)
{
    if (!length) {
        throw phase_exception("No more data for DATA OUT phase");
    }

    trace("Sending {0} byte(s) in DATA OUT phase", length);

    byte_count = bus.SendHandShake(buffer.data(), length);
    if (byte_count != length) {
        error("Sent {0} byte(s) in DATA OUT phase, expected size was {1} byte(s)", byte_count, length);
        throw phase_exception("DATA OUT phase failed");
    }

    length -= byte_count;
}

void InitiatorExecutor::MsgIn()
{
    const int msg = bus.MsgInHandShake();
    if (msg == -1) {
        error("MESSAGE IN phase failed");
    }
    else if (msg) {
        trace("Device did not report COMMAND COMPLETE, rejecting unsupported message ${:02x}", msg);

        next_message = 0x07;
    }
}

void InitiatorExecutor::MsgOut()
{
    array<uint8_t, 1> buf;

    // IDENTIFY or MESSAGE REJECT
    buf[0] = static_cast<uint8_t>(target_lun + next_message);


    if (bus.SendHandShake(buf.data(), buf.size()) != buf.size()) {
        error("MESSAGE OUT phase for {} failed", next_message == 0x80 ? "IDENTIFY" : "MESSAGE REJECT");
    }

    // Reset default message for MESSAGE OUT to IDENTIFY
    next_message = 0x80;
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
        if (const auto &it_status = STATUS_MAPPING.find(static_cast<status_code>(status)); it_status
            != STATUS_MAPPING.end()) {
            warn("Device reported {0} (status code ${1:02x})", it_status->second, status);
        }
        else if (status != 0xff) {
            warn("Device reported an unknown status (status code ${:02x})", status);
        }
        else {
            warn("Device did not respond");
        }
    }
}

