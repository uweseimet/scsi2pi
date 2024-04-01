//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <chrono>
#include <spdlog/spdlog.h>
#include "bus_factory.h"

bool Bus::Init(bool mode)
{
    static_assert(SIGNAL_CONTROL_MODE == 0 || SIGNAL_CONTROL_MODE == 2);

    target_mode = mode;

    return true;
}

int Bus::CommandHandShake(vector<uint8_t> &buf)
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
        return 0;
    }

    // The ICD AdSCSI ST, AdSCSI Plus ST and AdSCSI Micro ST host adapters allow SCSI devices to be connected
    // to the ACSI bus of Atari ST/TT computers and some clones. ICD-awarerrore drivers prepend a $1F byte in front
    // of the CDB (effectively resulting in a custom SCSI command) in order to get access to the full SCSI
    // command set. Native ACSI is limited to the low SCSI command classes with command bytes < $20.
    // Most other host adapters (e.g. LINK96/97 and the one by Inventronik) and also several devices (e.g.
    // UltraSatan or GigaFile) that can directly be connected to the Atari's ACSI port also support ICD
    // semantics. In fact, these semantics have become a standard in the Atari world.
    // SCSi2Pi becomes ICD compatible by ignoring the prepended $1F byte before processing the CDB.
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
            return 0;
        }
    }

    const int command_byte_count = BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(buf[0]));
    if (!command_byte_count) {
        EnableIRQ();

        // Unknown command
        return 0;
    }

    int offset = 0;

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; bytes_received++) {
        ++offset;

        SetREQ(true);

        ack = WaitSignal(PIN_ACK, true);

        WaitBusSettle();

        buf[offset] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitSignal(PIN_ACK, false)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// Initiator MESSAGE IN
int Bus::MsgInHandShake()
{
    const bus_phase phase = GetPhase();

    if (!WaitSignal(PIN_REQ, true)) {
        return -1;
    }

    // Phase error
    if (GetPhase() != phase) {
        return -1;
    }

    DisableIRQ();
    WaitBusSettle();
    EnableIRQ();

    const int msg = GetDAT();

    SetACK(true);

    // Request MESSAGE OUT phase for rejecting any unsupported message (only COMMAND COMPLETE is supported)
    if (msg) {
        SetATN(true);
    }

    WaitSignal(PIN_REQ, false);

    SetACK(false);

    return msg;
}

// Handshake for DATA IN and target MESSAGE IN
int Bus::ReceiveHandShake(uint8_t *buf, int count)
{
    int bytes_received;

    DisableIRQ();

    if (target_mode) {
        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            SetREQ(true);

            const bool ack = WaitSignal(PIN_ACK, true);

            WaitBusSettle();

            *buf = GetDAT();

            SetREQ(false);

            // Timeout waiting for ACK to change
            if (!ack || !WaitSignal(PIN_ACK, false)) {
                break;
            }

            buf++;
        }
    } else {
        const bus_phase phase = GetPhase();

        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            if (!WaitSignal(PIN_REQ, true)) {
                break;
            }

            // Phase error
            if (GetPhase() != phase) {
                break;
            }

            WaitBusSettle();

            *buf = GetDAT();

            SetACK(true);

            const bool req = WaitSignal(PIN_REQ, false);

            SetACK(false);

            if (!req || GetPhase() != phase) {
                break;
            }

            buf++;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// Handshake for DATA OUT and MESSAGE OUT
#ifdef BUILD_SCDP
int Bus::SendHandShake(const uint8_t *buf, int count, int daynaport_delay_after_bytes)
#else
int Bus::SendHandShake(const uint8_t *buf, int count, int)
#endif
{
    int bytes_sent;

    DisableIRQ();

    if (target_mode) {
        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
#ifdef BUILD_SCDP
            if (bytes_sent == daynaport_delay_after_bytes) {
                const timespec ts = { .tv_sec = 0, .tv_nsec = SCSI_DELAY_SEND_DATA_DAYNAPORT_NS };
                EnableIRQ();
                nanosleep(&ts, nullptr);
                DisableIRQ();
            }
#endif

            SetDAT(*buf);

            if (!WaitSignal(PIN_ACK, false)) {
                break;
            }

            SetREQ(true);

            const bool ack = WaitSignal(PIN_ACK, true);

            SetREQ(false);

            if (!ack) {
                break;
            }

            buf++;
        }

        WaitSignal(PIN_ACK, false);
    } else {
        const bus_phase phase = GetPhase();

        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
            SetDAT(*buf);

            if (!WaitSignal(PIN_REQ, true)) {
                break;
            }

            // Signal the last MESSAGE OUT byte
            if (phase == bus_phase::msgout && bytes_sent == count - 1) {
                SetATN(false);
            }

            // Phase error
            if (GetPhase() != phase) {
                break;
            }

            SetACK(true);

            const bool req = WaitSignal(PIN_REQ, false);

            SetACK(false);

            if (!req || GetPhase() != phase) {
                break;
            }

            buf++;
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
            spdlog::warn("Received RST signal during {} phase, aborting", GetPhaseName(GetPhase()));
            return false;
        }
    } while ((chrono::duration_cast < chrono::seconds > (chrono::steady_clock::now() - now).count()) < 3);

    spdlog::trace("Timeout while waiting for ACK/REQ to change to {}", state ? "true" : "false");

    return false;
}

bus_phase Bus::GetPhase()
{
    Acquire();

    if (GetSEL()) {
        return bus_phase::selection;
    }

    if (!GetBSY()) {
        return bus_phase::busfree;
    }

    // Get phase from bus signal lines
    return phases[(GetMSG() ? 0b100 : 0b000) | (GetCD() ? 0b010 : 0b000) | (GetIO() ? 0b001 : 0b000)];
}

// Phase Table with the phases based upon the MSG, C/D and I/O signals
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
constexpr array<bus_phase, 8> Bus::phases = {
    bus_phase::dataout,
    bus_phase::datain,
    bus_phase::command,
    bus_phase::status,
    bus_phase::reserved,
    bus_phase::reserved,
    bus_phase::msgout,
    bus_phase::msgin
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
    "???"
};
