//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include "generic_controller.h"

using namespace std;

class ScsiController : public GenericController
{

public:

    using GenericController::GenericController;
    ~ScsiController() override = default;

    void Reset() override;

    int GetEffectiveLun() const override;

    void BusFree() override;
    void MsgOut() override;

private:

    bool XferMsg(int) override;

    void ParseMessage() override;
    void ProcessMessage() override;
    void ProcessExtendedMessage() override;

    // The LUN from the IDENTIFY message
    int identified_lun = -1;

    bool atnmsg = false;

    // Message bytes and counter
    array<uint8_t, 256> msb = { };
    int msc = 0;
};

