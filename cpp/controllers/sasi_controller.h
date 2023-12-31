//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generic_controller.h"

class SasiController : public GenericController
{

public:

    using GenericController::GenericController;
    ~SasiController() override = default;

    int GetEffectiveLun() const override;

    void MsgOut() override;

private:

    bool XferMsg(int) override;

    void ParseMessage() override;
    void ProcessMessage() override;
    void ProcessExtendedMessage() override;
};
