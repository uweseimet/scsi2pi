//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// Host Services with support for realtime clock, shutdown and command execution
//
//---------------------------------------------------------------------------

//
// Features of the host services device:
//
// 1. Vendor-specific mode page 0x20 returns the current date and time, see mode_page_datetime
//
// 2. START/STOP UNIT shuts down s2p or shuts down/reboots the Raspberry Pi
//   a) !start && !load (STOP): Shut down s2p
//   b) !start && load (EJECT): Shut down the Raspberry Pi
//   c) start && load (LOAD): Reboot the Raspberry Pi
//
// 3. Remote command execution via SCSI, using these vendor-specific SCSI commands:
//
//   a) ExecuteOperation
//
// +==============================================================================
// |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
// |Byte |        |        |        |        |        |        |        |        |
// |=====+========================================================================
// | 0   |                           Operation code (c0h)                        |
// |-----+-----------------------------------------------------------------------|
// | 1   | Logical unit number      |     Reserved    |  TEXT  |  JSON  |  BIN   |
// |-----+-----------------------------------------------------------------------|
// | 2   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 3   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 4   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 5   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 6   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 7   | (MSB)                                                                 |
// |-----+---                        Byte transfer length                        |
// | 8   |                                                                 (LSB) |
// |-----+-----------------------------------------------------------------------|
// | 9   |                           Control                                     |
// +==============================================================================
//
//   b) ReceiveOperationResults
//
// +==============================================================================
// |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
// |Byte |        |        |        |        |        |        |        |        |
// |=====+========================================================================
// | 0   |                           Operation code (c1h)                        |
// |-----+-----------------------------------------------------------------------|
// | 1   | Logical unit number      |     Reserved    |  TEXT  |  JSON  |  BIN   |
// |-----+-----------------------------------------------------------------------|
// | 2   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 3   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 4   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 5   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 6   |                           Reserved                                    |
// |-----+-----------------------------------------------------------------------|
// | 7   | (MSB)                                                                 |
// |-----+---                        Byte transfer length                        |
// | 8   |                                                                 (LSB) |
// |-----+-----------------------------------------------------------------------|
// | 9   |                           Control                                     |
// +==============================================================================
//
// The remote interface commands that can be executed are defined in the s2p_interface.proto file.
// The BIN, JSON and TEXT flags control the input and output format of the protobuf data.
// Exactly one of them must be set. Input and output format do not have to be identical.
// ReceiveOperationResults returns the result of the last operation executed.
//

#include "host_services.h"
#include <chrono>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include "command/command_context.h"
#include "command/command_dispatcher.h"
#include "controllers/controller.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"

using namespace std::chrono;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace memory_util;
using namespace protobuf_util;

HostServices::HostServices(int lun) : PrimaryDevice(SCHS, scsi_level::spc_3, lun)
{
    SetProduct("Host Services");
    SetReady(true);
}

bool HostServices::SetUp()
{
    AddCommand(scsi_command::cmd_start_stop, [this]
        {
            StartStopUnit();
        });
    AddCommand(scsi_command::cmd_execute_operation, [this]
        {
            ExecuteOperation();
        });
    AddCommand(scsi_command::cmd_receive_operation_results, [this]
        {
            ReceiveOperationResults();
        });

    page_handler = make_unique<PageHandler>(*this, false, false);

    return true;
}

vector<uint8_t> HostServices::InquiryInternal() const
{
    return HandleInquiry(device_type::processor, false);
}

void HostServices::StartStopUnit() const
{
    const bool load = GetCdbByte(4) & 0x02;

    if (const bool start = GetCdbByte(4) & 0x01; !start) {
        GetController()->ScheduleShutdown(load ? shutdown_mode::stop_pi : shutdown_mode::stop_s2p);
    }
    else if (load) {
        GetController()->ScheduleShutdown(shutdown_mode::restart_pi);
    }
    else {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    StatusPhase();
}

void HostServices::ExecuteOperation()
{
    execution_results.erase(GetController()->GetInitiatorId());

    input_format = ConvertFormat();

    const int length = GetCdbInt16(7);
    if (!length) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    GetController()->SetTransferSize(length, length);

    DataOutPhase(length);
}

void HostServices::ReceiveOperationResults()
{
    const protobuf_format output_format = ConvertFormat();

    const auto &it = execution_results.find(GetController()->GetInitiatorId());
    if (it == execution_results.end()) {
        throw scsi_exception(sense_key::aborted_command, asc::host_services_receive_operation_results);
    }
    const string &execution_result = it->second;

    string data;
    switch (output_format) {
    case protobuf_format::binary:
        data = execution_result;
        break;

    case protobuf_format::json: {
        PbResult result;
        result.ParseFromArray(execution_result.data(), static_cast<int>(execution_result.size()));
        (void)MessageToJsonString(result, &data).ok();
        break;
    }

    case protobuf_format::text: {
        PbResult result;
        result.ParseFromArray(execution_result.data(), static_cast<int>(execution_result.size()));
        TextFormat::PrintToString(result, &data);
        break;
    }

    default:
        assert(false);
        break;
    }

    execution_results.erase(GetController()->GetInitiatorId());

    const auto allocation_length = static_cast<size_t>(GetCdbInt16(7));
    const auto length = static_cast<int>(min(allocation_length, data.size()));
    if (!length) {
        StatusPhase();
    }
    else {
        GetController()->CopyToBuffer(data.data(), length);

        DataInPhase(length);
    }
}

int HostServices::ModeSense6(cdb_t cdb, vector<uint8_t> &buf) const
{
    // Block descriptors cannot be returned
    if (!(cdb[1] & 0x08)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
    fill_n(buf.begin(), length, 0);

    const int size = page_handler->AddModePages(cdb, buf, 4, length, 255);

    // The size field does not count itself
    buf[0] = (uint8_t)(size - 1);

    return size;
}

int HostServices::ModeSense10(cdb_t cdb, vector<uint8_t> &buf) const
{
    // Block descriptors cannot be returned
    if (!(cdb[1] & 0x08)) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
    fill_n(buf.begin(), length, 0);

    const int size = page_handler->AddModePages(cdb, buf, 8, length, 65535);

    // The size fields do not count themselves
    SetInt16(buf, 0, size - 2);

    return size;
}

void HostServices::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    if (page == 0x20 || page == 0x3f) {
        AddRealtimeClockPage(pages, changeable);
    }
}

void HostServices::AddRealtimeClockPage(map<int, vector<byte>> &pages, bool changeable) const
{
    pages[32] = vector<byte>(sizeof(mode_page_datetime) + 2);

    if (!changeable) {
        const time_t &t = system_clock::to_time_t(system_clock::now());
        tm localtime;
        localtime_r(&t, &localtime);

        mode_page_datetime datetime;
        datetime.major_version = 0x01;
        datetime.minor_version = 0x00;
        datetime.year = static_cast<uint8_t>(localtime.tm_year);
        datetime.month = static_cast<uint8_t>(localtime.tm_mon);
        datetime.day = static_cast<uint8_t>(localtime.tm_mday);
        datetime.hour = static_cast<uint8_t>(localtime.tm_hour);
        datetime.minute = static_cast<uint8_t>(localtime.tm_min);
        // Ignore leap second for simplicity
        datetime.second = static_cast<uint8_t>(localtime.tm_sec < 60 ? localtime.tm_sec : 59);

        memcpy(&pages[32][2], &datetime, sizeof(datetime));
    }
}

int HostServices::WriteData(span<const uint8_t> buf, scsi_command command)
{
    assert(command == scsi_command::cmd_execute_operation);
    if (command != scsi_command::cmd_execute_operation) {
        throw scsi_exception(sense_key::aborted_command);
    }

    const auto length = GetCdbInt16(7);
    if (!length) {
        execution_results[GetController()->GetInitiatorId()].clear();
        return 0;
    }

    PbCommand cmd;
    switch (input_format) {
    case protobuf_format::binary:
        if (!cmd.ParseFromArray(buf.data(), length)) {
            LogTrace("Failed to deserialize protobuf binary data");
            throw scsi_exception(sense_key::aborted_command);
        }
        break;

    case protobuf_format::json: {
        if (string c((const char*)buf.data(), length); !JsonStringToMessage(c, &cmd).ok()) {
            LogTrace("Failed to deserialize protobuf JSON data");
            throw scsi_exception(sense_key::aborted_command);
        }
        break;
    }

    case protobuf_format::text: {
        if (string c((const char*)buf.data(), length); !TextFormat::ParseFromString(c, &cmd)) {
            LogTrace("Failed to deserialize protobuf text format data");
            throw scsi_exception(sense_key::aborted_command);
        }
        break;
    }

    default:
        assert(false);
        throw scsi_exception(sense_key::aborted_command);
    }

    PbResult result;
    CommandContext context(cmd);
    context.SetLocale(protobuf_util::GetParam(cmd, "locale"));
    if (!dispatcher->DispatchCommand(context, result)) {
        LogTrace("Failed to execute " + PbOperation_Name(cmd.operation()) + " operation");
        throw scsi_exception(sense_key::aborted_command);
    }

    execution_results[GetController()->GetInitiatorId()] = result.SerializeAsString();

    return length;
}

HostServices::protobuf_format HostServices::ConvertFormat() const
{
    switch (GetCdbByte(1) & 0b00000111) {
    case 0x001:
        return protobuf_format::binary;
        break;

    case 0b010:
        return protobuf_format::json;
        break;

    case 0b100:
        return protobuf_format::text;
        break;

    default:
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }
}
