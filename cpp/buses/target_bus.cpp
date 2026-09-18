//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus.h"
#include "shared/command_meta_data.h"

int Bus::TargetCommandHandShake(data_in_t buf)
{
    assert(!buf.empty());

    DisableIRQ();

    SetREQ(true);

    bool ack = WaitHandShake(PIN_ACK_MASK, true);

    buf[0] = GetDAT();

    SetREQ(false);

    if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
        return FinishTransfer(-1);
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
            return FinishTransfer(-1);
        }
    }

    const int command_byte_count = CommandMetaData::GetInstance().GetByteCount(static_cast<ScsiCommand>(buf[0]));
    if (!command_byte_count || command_byte_count > static_cast<int>(buf.size())) {
        // Unknown command or too many command bytes
        return FinishTransfer(0);
    }

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; ++bytes_received) {
        SetREQ(true);

        ack = WaitHandShake(PIN_ACK_MASK, true);

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
            return FinishTransfer(-1);
        }
    }

    return FinishTransfer(bytes_received);
}

// For DATA OUT and MESSAGE OUT
int Bus::TargetReceiveHandShake(data_in_t buf)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    int bytes_received;
    for (bytes_received = 0; bytes_received < count; ++bytes_received) {
        SetREQ(true);

        const bool ack = WaitHandShake(PIN_ACK_MASK, true);

        buf[bytes_received] = GetDAT();

        SetREQ(false);

        if (!ack || !WaitHandShake(PIN_ACK_MASK, false)) {
            return FinishTransfer(bytes_received);
        }
    }

    return FinishTransfer(bytes_received);
}

// For DATA IN, MESSAGE IN and STATUS
int Bus::TargetSendHandShake(data_out_t buf, [[maybe_unused]] int daynaport_delay_after_bytes)
{
    const auto count = static_cast<int>(buf.size());

    DisableIRQ();

    int bytes_sent;
    for (bytes_sent = 0; bytes_sent < count; ++bytes_sent) {
#ifdef BUILD_SCDP
        if (bytes_sent == daynaport_delay_after_bytes) {
            // Wait for a Daynaport delay
            WaitNanoSeconds(true);
        }
#endif

        SetDAT(buf[bytes_sent]);
        WaitNanoSeconds(false);

        if (!WaitHandShake(PIN_ACK_MASK, false)) {
            return FinishTransfer(bytes_sent);
        }

        SetREQ(true);

        const bool ack = WaitHandShake(PIN_ACK_MASK, true);

        SetREQ(false);

        if (!ack) {
            return FinishTransfer(bytes_sent);
        }
    }

    WaitHandShake(PIN_ACK_MASK, false);

    return FinishTransfer(bytes_sent);
}
