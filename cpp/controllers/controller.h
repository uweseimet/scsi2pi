//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
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
    ~Controller() override = default;

    bool Process() override;

    void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information,
        scsi_defs::status status = scsi_defs::status::check_condition) override;
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

    void Execute();
    void Send();
    void Receive();
    void XferMsg(uint8_t);
    bool XferIn(vector<uint8_t>&);
    bool XferOut(bool);

    void ParseMessage();
    void ProcessMessage();
    void ProcessExtendedMessage();

    void LogCdb() const;

    // The LUN from the IDENTIFY message
    int identified_lun = -1;

    bool atn_msg = false;

    vector<uint8_t> msg_bytes;
};

