//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// Host Services with support for realtime clock, shutdown and command execution
//
//---------------------------------------------------------------------------

#pragma once

#include "page_handler.h"

class CommandDispatcher;

class HostServices : public PrimaryDevice
{

public:

    explicit HostServices(int);
    ~HostServices() override = default;

    string SetUp() override;

    string GetIdentifier() const override
    {
        return "Host Services";
    }

    vector<uint8_t> InquiryInternal() const override;

    int WriteData(cdb_t, data_out_t, int, int) override;

    void SetDispatcher(shared_ptr<CommandDispatcher> d)
    {
        dispatcher = d;
    }

protected:

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    using ModePageDateTime = struct __attribute__((packed)) {
        // Major and minor version of this data structure (e.g. 1.0)
        uint8_t major_version;
        uint8_t minor_version;
        // Current date and time, with daylight savings time adjustment applied
        uint8_t year;// year - 1900
        uint8_t month;// 0-11
        uint8_t day;// 1-31
        uint8_t hour;// 0-23
        uint8_t minute;// 0-59
        uint8_t second;// 0-59
    };

    void StartStopUnit() const;
    void ExecuteOperation();
    void ReceiveOperationResults();

    int ModeSense6(cdb_t, data_in_t) const override;
    int ModeSense10(cdb_t, data_in_t) const override;

    ProtobufFormat ConvertFormat() const;

    static void AddRealtimeClockPage(map<int, vector<byte>>&, bool);

    unique_ptr<PageHandler> page_handler;

    // Operation results per initiator
    unordered_map<int, string> execution_results;

    shared_ptr<CommandDispatcher> dispatcher;

    ProtobufFormat input_format = ProtobufFormat::BINARY;

    static constexpr int EXECUTE_BUFFER_SIZE = 65535;
};
