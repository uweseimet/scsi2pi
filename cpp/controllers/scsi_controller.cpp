//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "base/primary_device.h"
#include "scsi_controller.h"

void ScsiController::Reset()
{
    GenericController::Reset();

    identified_lun = -1;

    scsi = { };
}

void ScsiController::BusFree()
{
    if (!IsBusFree()) {
        // Initialize ATN message reception status
        scsi.atnmsg = false;

        identified_lun = -1;
    }

    GenericController::BusFree();
}

void ScsiController::MsgOut()
{
    if (!IsMsgOut()) {
        // Process the IDENTIFY message
        if (IsSelection()) {
            scsi.atnmsg = true;
            scsi.msc = 0;
            scsi.msb = { };
        }

        LogTrace("Message Out phase");
        SetPhase(phase_t::msgout);

        GetBus().SetMSG(true);
        GetBus().SetCD(true);
        GetBus().SetIO(false);

        // Data transfer is 1 byte x 1 block
        ResetOffset();
        SetLength(1);
        SetBlocks(1);

        return;
    }

    Receive();
}

bool ScsiController::XferMsg(int msg)
{
    assert(IsMsgOut());

    // Save message out data
    if (scsi.atnmsg) {
        scsi.msb[scsi.msc] = (uint8_t)msg;
        scsi.msc++;
        scsi.msc %= 256;
    }

    return true;
}

void ScsiController::ParseMessage()
{
    int i = 0;
    while (i < scsi.msc) {
        const uint8_t message_type = scsi.msb[i];

        if (message_type == 0x06) {
            LogTrace("Received ABORT message");
            BusFree();
            return;
        }

        if (message_type == 0x0C) {
            LogTrace("Received BUS DEVICE RESET message");
            scsi.syncoffset = 0;
            if (auto device = GetDeviceForLun(identified_lun); device) {
                device->DiscardReservation();
            }
            BusFree();
            return;
        }

        if (message_type >= 0x80) {
            identified_lun = static_cast<int>(message_type) & 0x1F;
            LogTrace(fmt::format("Received IDENTIFY message for LUN {}", identified_lun));
        }

        if (message_type == 0x01) {
            LogTrace("Received EXTENDED MESSAGE");

            // Check only when synchronous transfer is possible
            // TODO Is this needed?
            if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
                SetLength(1);
                SetBlocks(1);
                GetBuffer()[0] = 0x07;
                MsgIn();
                return;
            }

            scsi.syncperiod = scsi.msb[i + 3];
            if (scsi.syncperiod > MAX_SYNC_PERIOD) {
                scsi.syncperiod = MAX_SYNC_PERIOD;
            }

            scsi.syncoffset = scsi.msb[i + 4];
            if (scsi.syncoffset > MAX_SYNC_OFFSET) {
                scsi.syncoffset = MAX_SYNC_OFFSET;
            }

            // STDR response message generation
            SetLength(5);
            SetBlocks(1);
            GetBuffer()[0] = 0x01;
            GetBuffer()[1] = 0x03;
            GetBuffer()[2] = 0x01;
            GetBuffer()[3] = scsi.syncperiod;
            GetBuffer()[4] = scsi.syncoffset;
            MsgIn();
            return;
        }

        // Next message
        i++;
    }
}

void ScsiController::ProcessMessage()
{
    // Continue message out phase as long as ATN keeps asserting
    if (GetBus().GetATN()) {
        // Data transfer is 1 byte x 1 block
        ResetOffset();
        SetLength(1);
        SetBlocks(1);
        return;
    }

    if (scsi.atnmsg) {
        ParseMessage();
    }

    // Initialize ATN message reception status
    scsi.atnmsg = false;

    Command();
}

void ScsiController::ProcessExtendedMessage()
{
    // Completed sending response to extended message of IDENTIFY message
    if (scsi.atnmsg) {
        scsi.atnmsg = false;

        Command();
    } else {
        BusFree();
    }
}

int ScsiController::GetEffectiveLun() const
{
    // Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
    return identified_lun != -1 ? identified_lun : GetLun();
}
