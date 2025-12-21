//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include "device.h"
#include "shared/memory_util.h"
#include "shared/s2p_defs.h"

class AbstractController;

class PrimaryDevice : public Device
{
    friend class AbstractController;
    friend class PageHandler;

    using command = function<void()>;

public:

    using ProductData = struct {
        string vendor;
        string product;
        string revision;
    };

    string Init();
    virtual string SetUp() = 0;
    virtual void CleanUp()
    {
        // Override if cleanup work is required for a derived device
    }

    virtual void Dispatch(ScsiCommand);

    AbstractController* GetController() const
    {
        return controller;
    }

    ProductData GetProductData() const;
    string SetProductData(const ProductData&, bool);

    string GetPaddedName() const
    {
        return fmt::format("{0:8}{1:16}{2:4}", product_data.vendor, product_data.product, product_data.revision);
    }

    ScsiLevel GetScsiLevel() const
    {
        return level;
    }
    bool SetScsiLevel(ScsiLevel);
    bool SetResponseDataFormat(ScsiLevel);

    enum SenseKey GetSenseKey() const
    {
        return sense_key;
    }
    enum Asc GetAsc() const
    {
        return asc;
    }
    void SetStatus(SenseKey, Asc);
    void ResetStatus();

    int GetId() const override;

    virtual string GetIdentifier() const = 0;

    int GetDelayAfterBytes() const
    {
        return delay_after_bytes;
    }

    bool CheckReservation(int) const;
    void DiscardReservation();

    void Reset();

    virtual int ReadData(data_in_t)
    {
        // Devices that implement a DATA IN phase have to override this method

        return 0;
    }

    // For DATA OUT phase, except for MODE SELECT
    virtual int WriteData(cdb_t, data_out_t, int, int) = 0;

    virtual void ModeSelect(cdb_t, data_out_t, int);

    virtual void FlushCache()
    {
        // Devices with a cache have to override this method
    }

    // Devices providing statistics have to override this method
    virtual vector<PbStatistics> GetStatistics() const
    {
        return {};
    }

protected:

    PrimaryDevice(PbDeviceType t, int l, int delay = SEND_NO_DELAY) : Device(t, l), delay_after_bytes(delay)
    {
    }

    void AddCommand(ScsiCommand, const command&);

    vector<uint8_t> HandleInquiry(DeviceType, bool) const;
    virtual vector<uint8_t> InquiryInternal() const = 0;
    void CheckReady();

    virtual void Inquiry();
    virtual void RequestSense();
    void SendDiagnostic() const;

    virtual int ModeSense6(cdb_t, data_in_t) const
    {
        // Nothing to do in base class
        return 0;
    }
    virtual int ModeSense10(cdb_t, data_in_t) const
    {
        // Nothing to do in base class
        return 0;
    }
    virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const
    {
        // Nothing to do in base class
    }

    void SetFilemark();
    void SetEom(Ascq);
    void SetIli();
    void SetInformation(int32_t);

    void StatusPhase() const;
    void DataInPhase(int) const;
    void DataOutPhase(int) const;

    int GetCdbByte(int) const;
    int GetCdbInt16(int) const;
    int GetCdbInt24(int) const;
    uint32_t GetCdbInt32(int) const;
    uint64_t GetCdbInt64(int) const;

private:

    static constexpr int NOT_RESERVED = -2;

    void SetController(AbstractController*);

    void ReportLuns() const;

    vector<byte> HandleRequestSense() const;

    ProductData product_data = ProductData(
        { "SCSI2Pi", "", fmt::format("{0:02}{1:1}{2:1}", s2p_major_version, s2p_minor_version, s2p_revision) });

    ScsiLevel level = ScsiLevel::NONE;
    ScsiLevel response_data_format = ScsiLevel::SCSI_1_CCS;

    SenseKey sense_key = SenseKey::NO_SENSE;
    Asc asc = Asc::NO_ADDITIONAL_SENSE_INFORMATION;
    Ascq eom = Ascq::NONE;

    bool valid = false;
    bool filemark = false;
    bool ili = false;
    int32_t information = 0;

    // Owned by the controller factory
    AbstractController *controller = nullptr;

    array<command, 256> commands = { };

    // Number of bytes during a transfer after which to delay for the Mac DaynaPort driver
    int delay_after_bytes = SEND_NO_DELAY;

    int reserving_initiator = NOT_RESERVED;
};
