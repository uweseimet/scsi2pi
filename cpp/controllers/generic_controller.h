//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
// Abstract base class for SCSI-like controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "abstract_controller.h"

class GenericController : public AbstractController
{

public:

    using AbstractController::AbstractController;
    ~GenericController() override = default;

    void Reset() override;

    bool Process(int) override;

    void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information,
        scsi_defs::status status = scsi_defs::status::check_condition) override;

    int GetInitiatorId() const override
    {
        return initiator_id;
    }

    void BusFree() override;
    void Selection() override;
    void Command() override;
    void MsgIn() override;
    void Status() override;
    void DataIn() override;
    void DataOut() override;

protected:

    bool XferIn(vector<uint8_t>&);
    bool XferOut(bool);
    void Receive();

    void Execute();
    void DataOutNonBlockOriented() const;
    void ProcessCommand();

    virtual void ParseMessage() = 0;
    virtual void ProcessMessage() = 0;
    virtual void ProcessExtendedMessage() = 0;

private:

    void Send();
    virtual bool XferMsg(int) = 0;
    bool XferOutBlockOriented(bool);
    void ReceiveBytes();

    // The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
    int initiator_id = UNKNOWN_INITIATOR_ID;
};

