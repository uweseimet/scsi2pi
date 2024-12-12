//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "abstract_controller.h"

class Controller : public AbstractController
{

public:

    using AbstractController::AbstractController;

    bool Process() override;

    void Error(sense_key, asc asc = asc::no_additional_sense_information, status_code status =
        status_code::check_condition) override;
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

    static int GetLunMax(bool sasi)
    {
        return sasi ? 2 : 32;
    }

private:

    void Execute();
    void Send();
    void Receive();
    void XferMsg();
    void TransferToHost();
    bool TransferFromHost(int);

    void ParseMessage();
    void ProcessMessage();
    void ProcessEndOfMessage();

    void RaiseDeferredError(sense_key, asc);
    void ProvideSenseData();

    // The LUN from the IDENTIFY message
    int identified_lun = -1;

    bool atn_msg = false;

    bool linked = false;

    bool flag = false;

    // For the last error reported by the controller, the controller and not the device has to provide the sense data.
    // This is required for SCSG because REQUEST SENSE is passed through to the actual device.
    sense_key deferred_sense_key = sense_key::no_sense;
    asc deferred_asc = asc::no_additional_sense_information;

    vector<uint8_t> msg_bytes;
};

