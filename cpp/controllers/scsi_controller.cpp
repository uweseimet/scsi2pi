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

    atnmsg = false;
}

void ScsiController::BusFree()
{
    if (!IsBusFree()) {
        // Initialize ATN message reception status
        atnmsg = false;

        identified_lun = -1;
    }

    GenericController::BusFree();
}

void ScsiController::MsgOut()
{
    if (!IsMsgOut()) {
        // Process the IDENTIFY message
        if (IsSelection()) {
            atnmsg = true;
            msc = 0;
            msb = { };
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
    if (atnmsg) {
        msb[msc] = (uint8_t)msg;
        msc++;
        msc %= 256;
    }

    return true;
}

void ScsiController::ParseMessage()
{
    int i = 0;
    while (i < msc) {
        const uint8_t message_type = msb[i];

        if (message_type == 0x06) {
            LogTrace("Received ABORT message");
            BusFree();
            return;
        }

        if (message_type == 0x0C) {
            LogTrace("Received BUS DEVICE RESET message");
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

            SetLength(1);
            SetBlocks(1);
            // MESSSAGE REJECT
            GetBuffer()[0] = 0x07;
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

    if (atnmsg) {
        ParseMessage();
    }

    // Initialize ATN message reception status
    atnmsg = false;

    Command();
}

void ScsiController::ProcessExtendedMessage()
{
    // Completed sending response to extended message of IDENTIFY message
    if (atnmsg) {
        atnmsg = false;

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
