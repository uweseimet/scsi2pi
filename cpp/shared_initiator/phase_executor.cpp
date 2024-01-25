//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <chrono>
#include "phase_executor.h"

using namespace std;
using namespace spdlog;

void PhaseExecutor::Reset() const
{
    bus.SetDAT(0);
    bus.SetBSY(false);
    bus.SetSEL(false);
    bus.SetATN(false);
}

bool PhaseExecutor::Execute(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length, bool sasi)
{
    status = 0;
    byte_count = 0;

    if (const auto &command = COMMAND_MAPPING.find(cmd); command != COMMAND_MAPPING.end()) {
        trace("Executing command {0} for target {1}:{2}", command->second.second, target_id, target_lun);
    }
    else {
        trace("Executing command ${0:02x} for target {1}:{2}", static_cast<int>(cmd), target_id, target_lun);
    }

    // There is no arbitration phase with SASI
    if (!sasi && !Arbitration()) {
        bus.Reset();
        return false;
    }

    if (!Selection(sasi)) {
        Reset();
        return false;
    }

    // Timeout 3 s
    auto now = chrono::steady_clock::now();
    while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3) {
        bus.Acquire();

        if (bus.GetREQ()) {
            try {
                if (Dispatch(cmd, cdb, buffer, length)) {
                    now = chrono::steady_clock::now();
                }
                else {
                    bus.Reset();
                    return !status;
                }
            }
            catch (const phase_exception &e) {
                error(e.what());
                bus.Reset();
                return false;
            }
        }
    }

    return false;
}

bool PhaseExecutor::Dispatch(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length)
{
    const phase_t phase = bus.GetPhase();

    trace("Handling {} phase", Bus::GetPhaseName(phase));

    switch (phase) {
    case phase_t::command:
        Command(cmd, cdb);
        break;

    case phase_t::status:
        Status();
        break;

    case phase_t::datain:
        DataIn(buffer, length);
        break;

    case phase_t::dataout:
        DataOut(buffer, length);
        break;

    case phase_t::msgin:
        MsgIn();
        // Done with this command cycle
        return false;

    case phase_t::msgout:
        MsgOut();
        break;

    default:
        warn("Ignoring {} phase", Bus::GetPhaseName(phase));
        return false;
    }

    return true;
}

bool PhaseExecutor::Arbitration() const
{
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

bool PhaseExecutor::Selection(bool sasi) const
{
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
        trace("SELECTION phase failed");
        return false;
    }

    Sleep(DESKEW_DELAY);
    Sleep(DESKEW_DELAY);

    bus.SetSEL(false);

    return true;
}

void PhaseExecutor::Command(scsi_command cmd, span<uint8_t> cdb) const
{
    cdb[0] = static_cast<uint8_t>(cmd);
    if (target_lun < 8) {
        // Encode LUN in the CDB for backwards compatibility with SCSI-1-CCS
        cdb[1] = static_cast<uint8_t>(cdb[1] + (target_lun << 5));
    }

    if (static_cast<int>(cdb.size()) != bus.SendHandShake(cdb.data(), static_cast<int>(cdb.size()))) {
        const auto &command = COMMAND_MAPPING.find(cmd);
        if (command != COMMAND_MAPPING.end()) {
            error("Command {} failed", command->second.second);
        }
        else {
            error("Command ${:02x} failed", static_cast<int>(cmd));
        }
    }
}

void PhaseExecutor::Status()
{
    array<uint8_t, 1> buf;

    if (bus.ReceiveHandShake(buf.data(), 1) != static_cast<int>(buf.size())) {
        error("STATUS phase failed");
    }
    else {
        status = buf[0];
    }
}

void PhaseExecutor::DataIn(span<uint8_t> buffer, int length)
{
    byte_count = bus.ReceiveHandShake(buffer.data(), length);
    if (byte_count > length) {
        warn("Received {0} byte(s) in DATA IN phase, provided size was {0} bytes", byte_count, length);
    }
    else {
        trace("Received {0} byte(s) in DATA IN phase, provided size was {0} bytes", byte_count, length);
    }
}

void PhaseExecutor::DataOut(span<uint8_t> buffer, int length)
{
    byte_count = bus.SendHandShake(buffer.data(), length);
    if (byte_count > length) {
        warn("Sent {0} byte(s) in DATA OUT phase, provided size was {0} bytes", byte_count, length);
    }
    else {
        trace("Sent {0} byte(s) in DATA OUT phase, provided size was {0} bytes", byte_count, length);
    }
}

void PhaseExecutor::MsgIn()
{
    array<uint8_t, 1> buf = { };

    if (bus.ReceiveHandShake(buf.data(), buf.size()) != buf.size()) {
        error("MESSAGE IN phase failed");
    }
    else if (buf[0]) {
        warn("MESSAGE IN did not report COMMAND COMPLETE, rejecting unsupported message ${:02x}", buf[0]);

        reject = true;

        // Request MESSAGE OUT for REJECT MESSAGE
        bus.SetATN(true);
    }
}

void PhaseExecutor::MsgOut()
{
    array<uint8_t, 1> buf;

    // IDENTIFY or MESSAGE REJECT
    buf[0] = static_cast<uint8_t>(target_lun | (reject ? 0x07 : 0x80));

    // Reset default to IDENTIFY
    reject = false;

    if (bus.SendHandShake(buf.data(), buf.size()) != buf.size()) {
        error("MESSAGE OUT phase for IDENTIFY failed");
    }
}

bool PhaseExecutor::WaitForFree() const
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

bool PhaseExecutor::WaitForBusy() const
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

void PhaseExecutor::SetTarget(int id, int lun)
{
    target_id = id;
    target_lun = lun;
}
