//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include "shared/command_meta_data.h"

bool Bus::Init(bool target)
{
    target_mode = target;

    return true;
}

int Bus::CommandHandShake(data_in_t buf)
{
    DisableIRQ();

    SetREQ(true);

    bool ack = WaitSignal(PIN_ACK, true);

    WaitBusSettle();

    buf[0] = GetDAT();

    SetREQ(false);

    // Timeout waiting for ACK to change
    if (!ack || !WaitSignal(PIN_ACK, false)) {
        EnableIRQ();
        return -1;
    }

    // The ICD AdSCSI ST, AdSCSI Plus ST and AdSCSI Micro ST host adapters allow SCSI devices to be connected
    // to the ACSI bus of Atari ST/TT computers and some clones. ICD-awarerrore drivers prepend a $1F byte in front
    // of the CDB (effectively resulting in a custom SCSI command) in order to get access to the full SCSI
    // command set. Native ACSI is limited to the low SCSI command classes with command bytes < $20.
    // Most other host adapters (e.g. LINK96/97 and the one by Inventronik) and also several devices (e.g.
    // UltraSatan or GigaFile) that can directly be connected to the Atari's ACSI port also support ICD
    // semantics. In fact, these semantics have become a standard in the Atari world.
    if (buf[0] == 0x1f) {
        SetREQ(true);

        ack = WaitSignal(PIN_ACK, true);

        WaitBusSettle();

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitSignal(PIN_ACK, false)) {
            EnableIRQ();
            return -1;
        }
    }

    const int command_byte_count = CommandMetaData::GetInstance().GetByteCount(static_cast<ScsiCommand>(buf[0]));
    if (!command_byte_count) {
        EnableIRQ();

        // Unknown command
        return 0;
    }

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; ++bytes_received) {
        SetREQ(true);

        ack = WaitSignal(PIN_ACK, true);

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitSignal(PIN_ACK, false)) {
            EnableIRQ();
            return -1;
        }
    }

    EnableIRQ();

    return bytes_received;
}

int Bus::InitiatorMsgInHandShake()
{
    if (const BusPhase phase = GetPhase(); !WaitSignal(PIN_REQ, true) || GetPhase() != phase) {
        return -1;
    }

    DisableIRQ();
    WaitBusSettle();
    EnableIRQ();

    const int msg = GetDAT();

    SetACK(true);

    // Request MESSAGE OUT phase for rejecting any unsupported message
    if (msg != static_cast<int>(MessageCode::COMMAND_COMPLETE)
        && msg != static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE)
        && msg != static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG)
        && msg != static_cast<int>(MessageCode::MESSAGE_REJECT)) {
        SetATN(true);
    }

    WaitSignal(PIN_REQ, false);

    SetACK(false);

    return msg;
}

// For DATA OUT and MESSAGE OUT
int Bus::TargetReceiveHandShake(data_in_t buf)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        SetREQ(true);

        const bool ack = WaitSignal(PIN_ACK, true);

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitSignal(PIN_ACK, false)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// For DATA IN and STATUS
int Bus::InitiatorReceiveHandShake(data_in_t buf)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    const BusPhase phase = GetPhase();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        if (!WaitSignal(PIN_REQ, true) || GetPhase() != phase) {
            break;
        }

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetACK(true);

        const bool req = WaitSignal(PIN_REQ, false);

        SetACK(false);

        if (!req || GetPhase() != phase) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// For DATA IN, MESSAGE IN and STATUS
#ifdef BUILD_SCDP
int Bus::TargetSendHandShake(data_out_t buf, int daynaport_delay_after_bytes)
#else
int Bus::TargetSendHandShake(data_out_t buf, int)
#endif
{
    const auto count = static_cast<int>(buf.size());

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
#ifdef BUILD_SCDP
        if (bytes_sent == daynaport_delay_after_bytes) {
            const timespec ts = { .tv_sec = 0, .tv_nsec = DAYNAPORT_SEND_DELAY_NS };
            EnableIRQ();
            nanosleep(&ts, nullptr);
            DisableIRQ();
        }
#endif

        SetDAT(buf[bytes_sent]);

        if (!WaitSignal(PIN_ACK, false)) {
            EnableIRQ();
            return bytes_sent;
        }

        SetREQ(true);

        const bool ack = WaitSignal(PIN_ACK, true);

        SetREQ(false);

        if (!ack) {
            break;
        }
    }

    WaitSignal(PIN_ACK, false);

    EnableIRQ();

    return bytes_sent;
}

// For MESSAGE OUT, DATA OUT and COMMAND
int Bus::InitiatorSendHandShake(data_out_t buf)
{
    const auto count = static_cast<int>(buf.size());

    const BusPhase phase = GetPhase();

    // Position of the last message byte if in MESSAGE OUT phase
    const int last_msg_out = phase == BusPhase::MSG_OUT ? count - 1 : -1;

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
        SetDAT(buf[bytes_sent]);

        if (!WaitSignal(PIN_REQ, true)) {
            break;
        }

        // Signal the last MESSAGE OUT byte when in MESSAGE OUT phase
        if (bytes_sent == last_msg_out) {
            SetATN(false);
        }

        if (GetPhase() != phase) {
            break;
        }

        SetACK(true);

        const bool req = WaitSignal(PIN_REQ, false);

        SetACK(false);

        if (!req || GetPhase() != phase) {
            break;
        }
    }

    EnableIRQ();

    return bytes_sent;
}

bool Bus::WaitSignal(int pin, bool state)
{
    const auto now = chrono::steady_clock::now();

    // Wait for up to 3 s
    do {
        Acquire();

        if (GetSignal(pin) == state) {
            return true;
        }

        if (GetRST()) {
            spdlog::warn("{0} received RST signal during {1} phase, aborting", target_mode ? "Target" : "Initiator",
                GetPhaseName(GetPhase()));
            return false;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    spdlog::trace("Timeout while waiting for {0} to become {1}", pin == PIN_ACK ? "ACK" : "REQ",
        state ? "true" : "false");

    return false;
}

BusPhase Bus::GetPhase()
{
    Acquire();

    if (GetSEL()) {
        return BusPhase::SELECTION;
    }

    if (!GetBSY()) {
        return BusPhase::BUS_FREE;
    }

    // Get phase from bus signal lines
    return phases[(GetMSG() ? 0b100 : 0b000) | (GetCD() ? 0b010 : 0b000) | (GetIO() ? 0b001 : 0b000)];
}

bool Bus::WaitForNotBusy()
{
    Acquire();

    // Wait up to 3 s for BSY to be released, signalling the end of the ARBITRATION phase
    if (GetBSY()) {
        const auto now = chrono::steady_clock::now();
        while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3) {
            Acquire();
            if (!GetBSY()) {
                return true;
            }
        }

        return false;
    }

    return true;
}

// Phase table with the phases based upon the MSG, C/D and I/O signals
//
// |MSG|C/D|I/O| Phase
// | 0 | 0 | 0 | DATA OUT
// | 0 | 0 | 1 | DATA IN
// | 0 | 1 | 0 | COMMAND
// | 0 | 1 | 1 | STATUS
// | 1 | 0 | 0 | RESERVED
// | 1 | 0 | 1 | RESERVED
// | 1 | 1 | 0 | MESSAGE OUT
// | 1 | 1 | 1 | MESSAGE IN
//
constexpr array<BusPhase, 8> Bus::phases = {
    BusPhase::DATA_OUT,
    BusPhase::DATA_IN,
    BusPhase::COMMAND,
    BusPhase::STATUS,
    BusPhase::RESERVED,
    BusPhase::RESERVED,
    BusPhase::MSG_OUT,
    BusPhase::MSG_IN
};

const array<string, 11> Bus::phase_names = {
    "BUS FREE",
    "ARBITRATION",
    "SELECTION",
    "RESELECTION",
    "COMMAND",
    "DATA IN",
    "DATA OUT",
    "STATUS",
    "MESSAGE IN",
    "MESSAGE OUT",
    "????"
};
