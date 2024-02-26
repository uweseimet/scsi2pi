//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include "interfaces/scsi_primary_commands.h"
#include "controllers/abstract_controller.h"
#include "device.h"

using namespace scsi_defs;

class PrimaryDevice : private ScsiPrimaryCommands, public Device
{
    friend class AbstractController;

    using command = function<void()>;

public:

    ~PrimaryDevice() override = default;

    virtual bool Init(const param_map&);
    virtual void CleanUp()
    {
        // Override if cleanup work is required for a derived device
    }

    virtual void Dispatch(scsi_command);

    scsi_level GetScsiLevel() const
    {
        return level;
    }
    bool SetScsiLevel(scsi_level);

    scsi_defs::sense_key GetSenseKey() const
    {
        return sense_key;
    }
    scsi_defs::asc GetAsc() const
    {
        return asc;
    }
    void SetStatus(scsi_defs::sense_key s, scsi_defs::asc a)
    {
        sense_key = s;
        asc = a;
    }

    int GetId() const override;

    int GetDelayAfterBytes() const
    {
        return delay_after_bytes;
    }

    bool CheckReservation(int, scsi_command, bool) const;
    void DiscardReservation();

    void Reset() override;

    virtual int ReadData(span<uint8_t>)
    {
        // Devices that implement a DATA IN phase have to override this method

        assert(false);
        return 0;
    }

    virtual int WriteData(span<const uint8_t>, scsi_command)
    {
        // Devices that implement a DATA OUT phase have to override this method, except for MODE SELECT

        assert(false);
        return 0;
    }

    virtual void FlushCache()
    {
        // Devices with a cache have to override this method
    }

    // Devices providing statistics have to override this method
    virtual vector<PbStatistics> GetStatistics() const
    {
        return vector<PbStatistics>();
    }

protected:

    PrimaryDevice(PbDeviceType type, scsi_level l, int lun, int delay = Bus::SEND_NO_DELAY)
    : Device(type, lun), level(l), delay_after_bytes(delay)
    {
    }

    void AddCommand(scsi_command cmd, const command &c)
    {
        commands[static_cast<int>(cmd)] = c;
    }

    vector<uint8_t> HandleInquiry(scsi_defs::device_type, bool) const;
    virtual vector<uint8_t> InquiryInternal() const = 0;
    void CheckReady();

    void Inquiry() override;
    void RequestSense() override;

    void SendDiagnostic() override;
    void ReserveUnit() override;
    void ReleaseUnit() override;

    void StatusPhase() const
    {
        controller->Status();
    }
    void DataInPhase(int length) const
    {
        controller->SetCurrentLength(length);
        controller->DataIn();
    }
    void DataOutPhase(int length) const
    {
        controller->SetCurrentLength(length);
        controller->DataOut();
    }

    inline auto GetController() const
    {
        return controller;
    }

    void LogTrace(const string &s) const
    {
        device_logger.Trace(s);
    }
    void LogDebug(const string &s) const
    {
        device_logger.Debug(s);
    }
    void LogWarn(const string &s) const
    {
        device_logger.Warn(s);
    }
    void LogError(const string &s) const
    {
        device_logger.Error(s);
    }

private:

    static constexpr int NOT_RESERVED = -2;

    void SetController(AbstractController*);

    void TestUnitReady() override;
    void ReportLuns() override;

    vector<byte> HandleRequestSense() const;

    DeviceLogger device_logger;

    scsi_level level = scsi_level::none;

    scsi_defs::sense_key sense_key = scsi_defs::sense_key::no_sense;
    scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information;

    // Owned by the controller factory
    AbstractController *controller = nullptr;

    array<command, 256> commands = { };

    // Number of bytes during a transfer after which to delay for the DaynaPort driver
    int delay_after_bytes = Bus::SEND_NO_DELAY;

    int reserving_initiator = NOT_RESERVED;
};
