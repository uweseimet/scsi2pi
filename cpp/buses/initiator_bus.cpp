//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus.h"

int Bus::InitiatorMsgInHandShake() const
{
    if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(BusPhase::MSG_IN)) {
        return -1;
    }

    const int msg = GetDAT();

    SetACK(true);

    // Request MESSAGE OUT phase for rejecting any unsupported message
    if (msg != to_underlying(MessageCode::COMMAND_COMPLETE)
        && msg != to_underlying(MessageCode::LINKED_COMMAND_COMPLETE)
        && msg != to_underlying(MessageCode::LINKED_COMMAND_COMPLETE_WITH_FLAG)
        && msg != to_underlying(MessageCode::MESSAGE_REJECT)) {
        SetATN(true);
    }

    WaitHandShake(PIN_REQ_MASK, false);

    SetACK(false);

    return msg;
}

// For DATA IN and STATUS
int Bus::InitiatorReceiveHandShake(data_in_t buf)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    const BusPhase phase = GetPhase();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(phase)) {
            return FinishTransfer(bytes_received);
        }

        buf[bytes_received] = GetDAT();

        SetACK(true);

        const bool req = WaitHandShake(PIN_REQ_MASK, false);

        SetACK(false);

        if (!req || !IsPhase(phase)) {
            return FinishTransfer(bytes_received);
        }
    }

    return FinishTransfer(bytes_received);
}

// For MESSAGE OUT, DATA OUT and COMMAND
int Bus::InitiatorSendHandShake(data_out_t buf)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    const BusPhase phase = GetPhase();

    // Position of the last message byte if in MESSAGE OUT phase
    const int last_msg_out = phase == BusPhase::MSG_OUT ? count - 1 : -1;

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
        SetDAT(buf[bytes_sent]);
        WaitNanoSeconds(false);

        if (!WaitHandShake(PIN_REQ_MASK, true) || !IsPhase(phase)) {
            return FinishTransfer(bytes_sent);
        }

        // Signal the last MESSAGE OUT byte when in MESSAGE OUT phase
        if (bytes_sent == last_msg_out) {
            SetATN(false);
        }

        SetACK(true);

        const bool req = WaitHandShake(PIN_REQ_MASK, false);

        SetACK(false);

        if (!req || !IsPhase(phase)) {
            return FinishTransfer(bytes_sent);
        }
    }

    return FinishTransfer(bytes_sent);
}
