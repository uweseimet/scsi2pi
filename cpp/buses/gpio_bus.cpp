//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <chrono>
#include "gpio_bus.h"

using namespace std;

bool GpioBus::Init(bool b)
{
    target_mode = b;

    return true;
}

int GpioBus::CommandHandShake(vector<uint8_t> &buf)
{
    DisableIRQ();

    SetREQ(true);

    bool ack = WaitACK(true);

    buf[0] = GetDAT();

    SetREQ(false);

    // Timeout waiting for ACK to change
    if (!ack || !WaitACK(false)) {
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

        ack = WaitACK(true);

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitACK(false)) {
            EnableIRQ();
            return 0;
        }
    }

    const int command_byte_count = GetCommandBytesCount(buf[0]);
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

        ack = WaitACK(true);

        buf[offset] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitACK(false)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// Initiator MESSAGE IN
int GpioBus::MsgInHandShake()
{
    const phase_t phase = GetPhase();

    // Check for timeout waiting for REQ signal
    if (!WaitREQ(true)) {
        return -1;
    }

    // Phase error
    // TODO Assumption: Phase does not change here, but only below
    if (GetPhase() != phase) {
        return -1;
    }

    const uint8_t msg = GetDAT();

    SetACK(true);

    // Request MESSAGE OUT phase for rejecting any unsupported message (only COMMAND COMPLETE is supported)
    if (msg) {
        SetATN(true);
    }

    WaitREQ(false);

    SetACK(false);

    return msg;
}

// Handshake for DATA IN and target MESSAGE IN
int GpioBus::ReceiveHandShake(uint8_t *buf, int count)
{
    int bytes_received;

    if (target_mode) {
        DisableIRQ();

        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            SetREQ(true);

            const bool ack = WaitACK(true);

            *buf = GetDAT();

            SetREQ(false);

            // Timeout waiting for ACK to change
            if (!ack || !WaitACK(false)) {
                break;
            }

            buf++;
        }

        EnableIRQ();
    } else {
        const phase_t phase = GetPhase();

        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            // Check for timeout waiting for REQ signal
            if (!WaitREQ(true)) {
                break;
            }

            // Phase error
            // TODO Assumption: Phase does not change here, but only below
            if (GetPhase() != phase) {
                break;
            }

            *buf = GetDAT();

            SetACK(true);

            const bool req = WaitREQ(false);

            SetACK(false);

            // Check for timeout waiting for REQ to clear and for unexpected phase change
            if (!req || GetPhase() != phase) {
                break;
            }

            buf++;
        }
    }

    return bytes_received;
}

// Handshake for DATA OUT and MESSAGE OUT
int GpioBus::SendHandShake(uint8_t *buf, int count, int daynaport_delay_after_bytes)
{
    int bytes_sent;

    if (target_mode) {
        DisableIRQ();

        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
            if (bytes_sent == daynaport_delay_after_bytes) {
                EnableIRQ();
                const timespec ts = { .tv_sec = 0, .tv_nsec = SCSI_DELAY_SEND_DATA_DAYNAPORT_NS };
                nanosleep(&ts, nullptr);
                DisableIRQ();
            }

            SetDAT(*buf);

            // Check for timeout waiting for ACK to clear
            if (!WaitACK(false)) {
                break;
            }

            SetREQ(true);

            const bool ack = WaitACK(true);

            SetREQ(false);

            // Check for timeout waiting for ACK to clear
            // TODO Do we really have to clear REQ here and everywhere else before checking this?
            if (!ack) {
                break;
            }

            buf++;
        }

        WaitACK(false);

        EnableIRQ();
    } else {
        const phase_t phase = GetPhase();

        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
            SetDAT(*buf);

            // Check for timeout waiting for REQ to be asserted
            if (!WaitREQ(true)) {
                break;
            }

            // Signal the last MESSAGE OUT byte
            if (phase == phase_t::msgout && bytes_sent == count - 1) {
                SetATN(false);
            }

            // Phase error
            // TODO Assumption: Phase does not change here, but only below
            if (GetPhase() != phase) {
                break;
            }

            SetACK(true);

            const bool req = WaitREQ(false);

            SetACK(false);

            // Check for timeout waiting for REQ to clear and for unexpected phase change
            if (!req || GetPhase() != phase) {
                break;
            }

            buf++;
        }
    }

    return bytes_sent;
}

bool GpioBus::WaitSignal(int pin, bool ast)
{
    const auto now = chrono::steady_clock::now();

    // Wait for up to 3 s
    do {
        Acquire();

        if (GetSignal(pin) == ast) {
            return true;
        }

        // Abort on a reset
        if (GetRST()) {
            return false;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    return false;
}
