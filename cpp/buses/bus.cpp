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

void Bus::Reset()
{
    // Set data bus signal directions
    SetDir(!target_mode);

    signals = 0xffffffff;
}

int Bus::CommandHandShake(span<uint8_t> buf)
{
    DisableIRQ();

    SetREQ(true);

    bool ack = WaitHandshake(PIN_ACK_MASK, true);

    WaitBusSettle();

    buf[0] = GetDAT();

    SetREQ(false);

    // Timeout waiting for ACK to change
    if (!ack || !WaitHandshake(PIN_ACK_MASK, false)) {
        return HandshakeTimeoutError();
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

        ack = WaitHandshake(PIN_ACK_MASK, true);

        WaitBusSettle();

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitHandshake(PIN_ACK_MASK, false)) {
            return HandshakeTimeoutError();
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

        ack = WaitHandshake(PIN_ACK_MASK, true);

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitHandshake(PIN_ACK_MASK, false)) {
            return HandshakeTimeoutError();
        }
    }

    EnableIRQ();

    return bytes_received;
}

int Bus::InitiatorMsgInHandShake()
{
    if (!WaitHandshake(PIN_REQ_MASK, true) || !IsPhase(BusPhase::MSG_IN)) {
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

    WaitHandshake(PIN_REQ_MASK, false);

    SetACK(false);

    return msg;
}

int Bus::TargetReceiveHandShake(uint8_t *buf, int count) // NOSONAR This should not be a span, buf and count are unrelated
{
    DisableIRQ();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        SetREQ(true);

        const bool ack = WaitHandshake(PIN_ACK_MASK, true);

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitHandshake(PIN_ACK_MASK, false)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// For DATA IN and STATUS
int Bus::InitiatorReceiveHandShake(uint8_t *buf, int count) // NOSONAR This should not be a span, buf and count are unrelated
{
    DisableIRQ();

    const BusPhase phase = GetPhase();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        if (!WaitHandshake(PIN_REQ_MASK, true) || !IsPhase(phase)) {
            break;
        }

        WaitBusSettle();

        buf[bytes_received] = GetDAT();

        SetACK(true);

        const bool req = WaitHandshake(PIN_REQ_MASK, false);

        SetACK(false);

        if (!req || !IsPhase(phase)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

#ifdef BUILD_SCDP
int Bus::TargetSendHandShake(const uint8_t *buf, int count, int daynaport_delay_after_bytes) // NOSONAR This should not be a span, buf and count are unrelated
#else
int Bus::TargetSendHandShake(const uint8_t *buf, int count, int)
#endif
{
    DisableIRQ();

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

        if (!WaitHandshake(PIN_ACK_MASK, false)) {
            return HandshakeTimeoutError();
        }

        SetREQ(true);

        const bool ack = WaitHandshake(PIN_ACK_MASK, true);

        SetREQ(false);

        if (!ack) {
            break;
        }
    }

    WaitHandshake(PIN_ACK_MASK, false);

    EnableIRQ();

    return bytes_sent;
}

// For MESSAGE OUT, DATA OUT and COMMAND
int Bus::InitiatorSendHandShake(const uint8_t *buf, int count) // NOSONAR This should not be a span, buf and count are unrelated
{
    DisableIRQ();

    const BusPhase phase = GetPhase();

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
        SetDAT(buf[bytes_sent]);

        if (!WaitHandshake(PIN_REQ_MASK, true)) {
            break;
        }

        // Signal the last MESSAGE OUT byte
        if (phase == BusPhase::MSG_OUT && bytes_sent == count - 1) {
            SetATN(false);
        }

        if (!IsPhase(phase)) {
            break;
        }

        SetACK(true);

        const bool req = WaitHandshake(PIN_REQ_MASK, false);

        SetACK(false);

        if (!req || !IsPhase(phase)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_sent;
}

bool Bus::WaitHandshake(int pin, bool state)
{
    assert(pin == PIN_REQ_MASK || pin == PIN_ACK_MASK);

    // Shortcut for the case where REQ/ACK is already in the required state
    Acquire();
    if (GetControl(pin) == state) {
        return true;
    }

    // Wait for REQ or ACK for up to 3 s
    const auto now = chrono::steady_clock::now();
    do {
        if (GetRST()) {
            spdlog::warn("{0} received RST signal during {1} phase, aborting", target_mode ? "Target" : "Initiator",
                GetPhaseName(GetPhase()));
            return false;
        }

        Acquire();
        if (GetControl(pin) == state) {
            return true;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    spdlog::trace("Timeout while waiting for ACK/REQ to change to {}", state ? "true" : "false");

    return false;
}

inline BusPhase Bus::GetPhase()
{
    Acquire();

    // Get phase from bus signal lines SEL, BSY, I/O, C/D and MSG
    return phases[(signals >> PIN_MSG) & 0b11111];
}

void Bus::SetIO(bool state)
{
    assert(target_mode);

    SetControl(PIN_IO, state);

    SetDir(state);
}

// Get input signal value (except for DP and DT0-DT7)
inline bool Bus::GetControl(int pinMask) const
{
    assert(pinMask >= PIN_ATN_MASK && pinMask <= PIN_SEL_MASK);

    // Invert because of negative logic (internal processing uses positive logic)
    return !(signals & pinMask);
}

inline uint8_t Bus::GetDAT()
{
    Acquire();

    return static_cast<uint8_t>(~(signals >> PIN_DT0));
}

int Bus::HandshakeTimeoutError()
{
    EnableIRQ();

    return -1;
}

// Phase table with the phases based upon the SEL, BSY, I/O, C/D and MSG signals (negative logic)
// |I/O|C/D|MSG| Phase
// | 0 | 0 | 0 | MESSAGE IN
// | 0 | 0 | 1 | STATUS
// | 0 | 1 | 0 | RESERVED
// | 0 | 1 | 1 | DATA IN
// | 1 | 0 | 0 | MESSAGE OUT
// | 1 | 0 | 1 | COMMAND
// | 1 | 1 | 0 | RESERVED
// | 1 | 1 | 1 | DATA OUT
constexpr array<BusPhase, 32> Bus::phases = {
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::SELECTION,
    BusPhase::MSG_IN,
    BusPhase::STATUS,
    BusPhase::RESERVED,
    BusPhase::DATA_IN,
    BusPhase::MSG_OUT,
    BusPhase::COMMAND,
    BusPhase::RESERVED,
    BusPhase::DATA_OUT,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE,
    BusPhase::BUS_FREE
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
