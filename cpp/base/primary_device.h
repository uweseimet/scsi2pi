//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
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
#include "s2p_defs.h"
#include "device.h"

class PrimaryDevice : public ScsiPrimaryCommands, public Device
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

    auto GetController() const
    {
        return controller;
    }

    scsi_level GetScsiLevel() const
    {
        return level;
    }
    bool SetScsiLevel(scsi_level);

    enum sense_key GetSenseKey() const
    {
        return sense_key;
    }
    enum asc GetAsc() const
    {
        return asc;
    }
    void SetStatus(enum sense_key s, enum asc a)
    {
        sense_key = s;
        asc = a;
    }
    void SetFilemark()
    {
        filemark = true;
    }
    void SetEom()
    {
        eom = true;
    }
    void SetInformation(uint32_t value)
    {
        information = value;
        valid = true;
    }

    int GetId() const override;

    int GetDelayAfterBytes() const
    {
        return delay_after_bytes;
    }

    bool CheckReservation(int) const;
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

    PrimaryDevice(PbDeviceType type, scsi_level l, int lun, int delay = SEND_NO_DELAY)
    : Device(type, lun), level(l), delay_after_bytes(delay)
    {
    }

    void AddCommand(scsi_command, const command&);

    vector<uint8_t> HandleInquiry(device_type, bool) const;
    virtual vector<uint8_t> InquiryInternal() const = 0;
    void CheckReady();

    void Inquiry() override;
    void RequestSense() override;

    void SendDiagnostic() override;
    void ReserveUnit() override;
    void ReleaseUnit() override;

    void StatusPhase() const;
    void DataInPhase(int) const;
    void DataOutPhase(int) const;

    void LogTrace(const string &s) const
    {
        device_logger.Trace(s);
    }
    void LogDebug(const string &s) const
    {
        device_logger.Debug(s);
    }
    void LogInfo(const string &s) const
    {
        device_logger.Info(s);
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

    enum sense_key sense_key = sense_key::no_sense;
    enum asc asc = asc::no_additional_sense_information;

    bool valid = false;
    bool filemark = false;
    bool eom = false;
    uint32_t information = 0;

    // Owned by the controller factory
    AbstractController *controller = nullptr;

    array<command, 256> commands = { };

    // Number of bytes during a transfer after which to delay for the Mac DaynaPort driver
    int delay_after_bytes = SEND_NO_DELAY;

    int reserving_initiator = NOT_RESERVED;
};
