//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "abstract_controller.h"

class Controller : public AbstractController
{

public:

    using AbstractController::AbstractController;

    bool Process() override;

    void Error(SenseKey, Asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION, StatusCode = StatusCode::CHECK_CONDITION) override;
    void Reset() override;

    void BusFree() override;
    void Selection() override;
    void Command() override;
    void MsgIn() override;
    void MsgOut() override;
    void Status() override;
    void DataIn() override;
    void DataOut() override;

    int GetEffectiveLun() const override;

private:

    void ResetFlags();

    void Execute();
    void Send();
    void Receive();
    void XferMsg();
    void TransferToHost();
    bool TransferFromHost(int);

    void ParseMessage();
    void ProcessMessage();
    void ProcessEndOfMessage();

    void RaiseDeferredError(SenseKey, Asc);
    void ProvideSenseData();

    // The LUN from the IDENTIFY message
    int identified_lun = -1;

    bool atn_msg = false;

    bool linked = false;

    bool flag = false;

    // For the last error reported by the controller, the controller and not the device has to provide the sense data.
    // This is required for SCSG because REQUEST SENSE is passed through to the actual device.
    SenseKey deferred_sense_key = SenseKey::NO_SENSE;
    Asc deferred_asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION;

    vector<uint8_t> msg_bytes;
};

