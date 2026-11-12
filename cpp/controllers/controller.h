//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "abstract_controller.h"

class Bus;
class ScriptGenerator;

class Controller : public AbstractController
{

public:

    Controller(Bus &b, int id, shared_ptr<ScriptGenerator> s, const S2pFormatter &formatter) : AbstractController(id,
        formatter), bus(b), script_generator(s)
    {
    }

    bool Process() override;

    void Error(SenseKey, Asc = NO_ADDITIONAL_SENSE_INFORMATION, StatusCode = CHECK_CONDITION) override;
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
    void ResetDeferredSenseData();

    void Execute(ScsiCommand);
    void Send();
    void Receive();
    void XferMsg();
    void TransferToHost();
    void TransferFromHost(int);

    void ParseMessage();
    void LogExtendedMessage(size_t) const;
    void ProcessMessage();
    void ProcessEndOfMessage();

    void BusDeviceReset();

    void RaiseDeferredError(SenseKey, Asc);
    void ProvideSenseData();

    // The device that reports an error, which is the addressed LUN or LUN 0 if there is no such LUN
    shared_ptr<PrimaryDevice> GetReportingDevice(Asc) const;

    Bus &bus;

    shared_ptr<ScriptGenerator> script_generator;

    // The LUN from the IDENTIFY message
    int identified_lun = -1;

    bool atn_msg = false;

    bool linked = false;

    bool link_flag = false;

    // For the last error reported by the controller, the controller and not the device has to provide the sense data.
    // This is required for SCSG because REQUEST SENSE is passed through to the actual device.
    SenseKey deferred_sense_key = NO_SENSE;
    Asc deferred_asc = NO_ADDITIONAL_SENSE_INFORMATION;
    // The LUN of the device that holds the same sense data as the deferred sense data
    int deferred_lun = 0;

    vector<uint8_t> msg_bytes;
};

