//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "base/primary_device.h"
#include "scsi_controller.h"

void ScsiController::Reset()
{
    GenericController::Reset();

    identified_lun = -1;

    atn_msg = false;
}

void ScsiController::BusFree()
{
    if (!IsBusFree()) {
        identified_lun = -1;

        atn_msg = false;
    }

    GenericController::BusFree();
}

void ScsiController::MsgOut()
{
    if (!IsMsgOut()) {
        LogTrace("MESSAGE OUT phase");

        // Process the IDENTIFY message
        if (IsSelection()) {
            atn_msg = true;
            msg_bytes.clear();
        }

        SetPhase(phase_t::msgout);

        GetBus().SetMSG(true);
        GetBus().SetCD(true);
        GetBus().SetIO(false);

        ResetOffset();
        SetCurrentLength(1);
        SetTransferSize(1, 1);

        return;
    }

    Receive();
}

void ScsiController::XferMsg(uint8_t msg)
{
    assert(IsMsgOut());

    if (atn_msg) {
        msg_bytes.emplace_back(msg);
    }
}

void ScsiController::ParseMessage()
{
    for (const uint8_t message : msg_bytes) {
        switch (message) {
        case 0x01: {
            LogTrace("Received EXTENDED MESSAGE");
            SetCurrentLength(1);
            SetTransferSize(1, 1);
            // MESSSAGE REJECT
            GetBuffer()[0] = 0x07;
            MsgIn();
            return;
        }

        case 0x06: {
            LogTrace("Received ABORT message");
            BusFree();
            return;
        }

        case 0x0c: {
            LogTrace("Received BUS DEVICE RESET message");
            if (auto device = GetDeviceForLun(GetEffectiveLun()); device) {
                device->DiscardReservation();
            }
            BusFree();
            return;
        }

        default:
            if (message >= 0x80) {
                identified_lun = static_cast<int>(message) & 0x1f;
                LogTrace(fmt::format("Received IDENTIFY message for LUN {}", identified_lun));
            }
            break;
        }
    }
}

void ScsiController::ProcessMessage()
{
    // MESSAGE OUT phase as long as ATN is asserted
    if (GetBus().GetATN()) {
        ResetOffset();
        SetCurrentLength(1);
        SetTransferSize(1, 1);
        return;
    }

    if (atn_msg) {
        atn_msg = false;
        ParseMessage();
    }

    Command();
}

void ScsiController::ProcessExtendedMessage()
{
    // Completed sending response to extended message of IDENTIFY message
    if (atn_msg) {
        atn_msg = false;
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
