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
#include <spdlog/spdlog.h>

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

bool Bus::WaitHandShake(int pin_mask, bool state) const
{
    assert(has_single_bit(static_cast<unsigned>(pin_mask)) && pin_mask >= PIN_ATN_MASK && pin_mask <= PIN_SEL_MASK);

    // Shortcut for the case where REQ/ACK is already in the required state
    Acquire();
    if (GetSignal(pin_mask) == state) {
        return true;
    }

    chrono::steady_clock::time_point deadline;
    for (unsigned n = 1;; ++n) {
        if (GetRST()) {
            warn("Received RST signal during {} phase, aborting", GetPhaseName(GetPhase()));
            return false;
        }

        Acquire();
        if (GetSignal(pin_mask) == state) {
            return true;
        }

        // Read the clock only every 256 polls because it may be expensive
        if (!(n & 0xff)) {
            const auto now = chrono::steady_clock::now();
            if (n == 0x100) {
                deadline = now + TIMEOUT_3_SECONDS;
            }
            else if (now >= deadline) {
                break;
            }
        }
    }

    trace("Timeout while waiting for {} to become {}", pin_mask == PIN_ACK_MASK ? "ACK" : "REQ",
        state ? "true" : "false");

    return false;
}

int Bus::FinishTransfer(int count)
{
    EnableIRQ();
    return count;
}

void Bus::SetBSY(bool state) const
{
    SetSignal(PIN_BSY, state);

    if (!state) {
        SetSignal(PIN_MSG, false);
        SetSignal(PIN_CD, false);
        SetSignal(PIN_REQ, false);
        SetIO(false);
    }
}

void Bus::SetIO(bool state) const
{
    SetSignal(PIN_IO, state);

    SetDataDirIn(state);
}

// Get input signal value (except for DP and DT0-DT7)
bool Bus::GetSignal(int pin_mask) const
{
    assert(pin_mask >= PIN_ATN_MASK && pin_mask <= PIN_SEL_MASK);

    // Invert because of negative logic (internal processing uses positive logic)
    return !(signals & pin_mask);
}

uint8_t Bus::GetSelection() const
{
    // Wait for BSY to be released, signalling the end of the ARBITRATION phase
    const auto now = chrono::steady_clock::now();
    const auto deadline = now + TIMEOUT_3_SECONDS;
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
