//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sasi_controller.h"

void SasiController::MsgOut()
{
    // SASI does not support a message out phase
    Command();
}

bool SasiController::XferMsg(int)
{
    assert(false);
    return false;
}

void SasiController::ParseMessage()
{
    assert(false);
}

void SasiController::ProcessMessage()
{
    assert(false);
}

void SasiController::ProcessExtendedMessage()
{
    BusFree();
}

int SasiController::GetEffectiveLun() const
{
    return GetLun();
}
