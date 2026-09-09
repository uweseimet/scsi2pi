//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus.h"
#include <bit>
#include <chrono>
#include <spdlog/spdlog.h>
#include "shared/command_meta_data.h"

using namespace spdlog;

bool Bus::Init(bool target)
{
    if (const string &error = SetUp(target); !error.empty()) {
        critical(error);
        return false;
    }

    Reset();

    return true;
}

void Bus::Reset() const
{
    signals = 0xffffffff;
}

int Bus::TargetCommandHandShake(data_in_t buf) const
{
    assert(!buf.empty());

    SetREQ(true);

    bool ack = WaitHandShake(PIN_ACK_MASK, true);

    buf[0] = GetDAT();

    SetREQ(false);

    if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
        return -1;
    }

    // The ICD AdSCSI ST, AdSCSI Plus ST and AdSCSI Micro ST host adapters allow SCSI devices to be connected
    // to the ACSI bus of Atari ST/TT computers and some clones. ICD-aware drivers prepend a $1F byte in front
    // of the CDB (effectively resulting in a custom SCSI command) in order to get access to the full SCSI
    // command set. Native ACSI is limited to the low SCSI command classes with command bytes < $20.
    // Most other host adapters (e.g. LINK96/97 and the one by Inventronik) and also several devices (e.g.
    // UltraSatan or GigaFile) that can directly be connected to the Atari's ACSI port also support ICD
    // semantics. In fact, these semantics have become a standard in the Atari world.
    if (buf[0] == 0x1f) {
        SetREQ(true);

        ack = WaitHandShake(PIN_ACK_MASK, true);

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(false);

        if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
            return -1;
        }
    }

    const int command_byte_count = CommandMetaData::GetInstance().GetByteCount(static_cast<ScsiCommand>(buf[0]));
    if (!command_byte_count) {
        // Unknown command
        return 0;
    }

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; ++bytes_received) {
        SetREQ(true);

        ack = WaitHandShake(PIN_ACK_MASK, true);

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
            return -1;
        }
    }

    return bytes_received;
}

int Bus::InitiatorMsgInHandShake() const
{
    if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(BusPhase::MSG_IN)) {
        return -1;
    }

    const int msg = GetDAT();

    SetACK(true);

    // Request MESSAGE OUT phase for rejecting any unsupported message
    if (msg != static_cast<int>(MessageCode::COMMAND_COMPLETE)
        && msg != static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE)
        && msg != static_cast<int>(MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG)
        && msg != static_cast<int>(MessageCode::MESSAGE_REJECT)) {
        SetATN(true);
    }

    WaitHandShake(PIN_REQ_MASK, false);

    SetACK(false);

    return msg;
}

// For DATA OUT and MESSAGE OUT
int Bus::TargetReceiveHandShake(data_in_t buf) const
{
    const auto count = static_cast<int>(buf.size());

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        SetREQ(true);

        const bool ack = WaitHandShake(PIN_ACK_MASK, true);

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
            break;
        }
    }

    return bytes_received;
}

// For DATA IN and STATUS
int Bus::InitiatorReceiveHandShake(data_in_t buf) const
{
    const auto count = static_cast<int>(buf.size());

    const BusPhase phase = GetPhase();

    auto receive_byte = [this, &phase](uint8_t &byte_out) {
        if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(phase)) {
            return false;
        }

        byte_out = GetDAT();

        SetACK(true);

        const bool req = WaitHandShake(PIN_REQ_MASK, false);

        SetACK(false);

        return req && IsPhase(phase);
    };

    int bytes_received = 0;
    while (bytes_received < count && receive_byte(buf[bytes_received])) {
        ++bytes_received;
    }

    return bytes_received;
}
// For DATA IN, MESSAGE IN and STATUS
int Bus::TargetSendHandShake(data_out_t buf, [[maybe_unused]] int daynaport_delay_after_bytes) const
{
    const auto count = static_cast<int>(buf.size());

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
#ifdef BUILD_SCDP
        if (bytes_sent == daynaport_delay_after_bytes) {
            // Wait for a Daynaport delay
            WaitNanoSeconds(true);
        }
#endif

        SetDAT(buf[bytes_sent]);

        if (!WaitHandShake(PIN_ACK_MASK, false)) {
            return bytes_sent;
        }

        SetREQ(true);

        const bool ack = WaitHandShake(PIN_ACK_MASK, true);

        SetREQ(false);

        if (!ack) {
            break;
        }
    }

    WaitHandShake(PIN_ACK_MASK, false);

    return bytes_sent;
}

// For MESSAGE OUT, DATA OUT and COMMAND
int Bus::InitiatorSendHandShake(data_out_t buf) const
{
    const auto count = static_cast<int>(buf.size());

    const BusPhase phase = GetPhase();
    const int last_msg_out = (phase == BusPhase::MSG_OUT) ? count - 1 : -1;

    auto send_byte = [this, &phase](uint8_t byte, bool is_last_msg) {
        SetDAT(byte);

        if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(phase)) {
            return false;
        }

        if (is_last_msg) {
            SetATN(false);
        }

        SetACK(true);

        const bool req = WaitHandShake(PIN_REQ_MASK, false);

        SetACK(false);

        return req && IsPhase(phase);
    };

    int bytes_sent = 0;
    while (bytes_sent < count && send_byte(buf[bytes_sent], bytes_sent == last_msg_out)) {
        ++bytes_sent;
    }

    return bytes_sent;
}

bool Bus::WaitHandShake(int pin_mask, bool state) const
{
    assert(has_single_bit(static_cast<unsigned>(pin_mask)) && pin_mask >= PIN_ATN_MASK && pin_mask <= PIN_SEL_MASK);

    // Shortcut for the case where REQ/ACK is already in the required state
    Acquire();
    if (GetSignal(pin_mask) == state) {
        return true;
    }

    // Wait up to 3 s
    const auto now = chrono::steady_clock::now();
    do {
        if (GetRST()) {
            warn("Received RST signal during {} phase, aborting", GetPhaseName(GetPhase()));
            return false;
        }

        Acquire();
        if (GetSignal(pin_mask) == state) {
            return true;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    trace("Timeout while waiting for {} to become {}", pin_mask == PIN_ACK_MASK ? "ACK" : "REQ",
        state ? "true" : "false");

    return false;
}

void Bus::SetBSY(bool state) const
{
    SetSignal(PIN_BSY, state);

    if (!state) {
        SetSignal(PIN_MSG, false);
        SetSignal(PIN_CD, false);
        SetSignal(PIN_REQ, false);
        SetSignal(PIN_IO, false);
    }
}

void Bus::SetIO(bool state) const
{
    SetSignal(PIN_IO, state);

    SetDataDirIn(state);
}

// Get input signal value (except for DP and DT0-DT7)
inline bool Bus::GetSignal(int pin_mask) const
{
    assert(pin_mask >= PIN_ATN_MASK && pin_mask <= PIN_SEL_MASK);

    // Invert because of negative logic (internal processing uses positive logic)
    return !(signals & pin_mask);
}

uint8_t Bus::GetSelection() const
{
    // Wait up to 3 s for BSY to be released, signalling the end of the ARBITRATION phase
    const auto now = chrono::steady_clock::now();
    const auto deadline = now + chrono::seconds(3);
    do {
        Acquire();
        if (!GetBSY()) {
            // Initiator and target ID
            return GetDAT();
        }
    } while (chrono::steady_clock::now() < deadline);

    return 0;
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
const array<BusPhase, 32> Bus::phases = {
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
