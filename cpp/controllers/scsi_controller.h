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
    // Transfer period factor (limited to 50 x 4 = 200ns)
    static const int MAX_SYNC_PERIOD = 50;

    // REQ/ACK offset(limited to 16)
    static const uint8_t MAX_SYNC_OFFSET = 16;

    // TODO Are data for synchronous transfers required at all?
    // The Pi does not support this. Consider rejecting related messages and removing this structure.
    using scsi_t = struct _scsi_t {
        // Synchronous transfer possible
        bool syncenable;
        // Synchronous transfer period
        uint8_t syncperiod = MAX_SYNC_PERIOD;
        // Synchronous transfer offset
        uint8_t syncoffset;
        // Number of synchronous transfer ACKs
        int syncack;

        bool atnmsg;

        int msc;
        array<uint8_t, 256> msb;
    };

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

    scsi_t scsi = { };
};

