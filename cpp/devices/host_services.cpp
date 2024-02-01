//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
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

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/text_format.h>
#include <algorithm>
#include <chrono>
#include "shared/shared_exceptions.h"
#include "protobuf/protobuf_util.h"
#include "controllers/scsi_controller.h"
#include "base/memory_util.h"
#include "host_services.h"
#include "generated/s2p_interface.pb.h"

using namespace std::chrono;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace s2p_interface;
using namespace memory_util;
using namespace protobuf_util;

HostServices::HostServices(int lun) : ModePageDevice(SCHS, scsi_level::spc_3, lun, false)
{
    SetProduct("Host Services");
}

bool HostServices::Init(const param_map &params)
{
    ModePageDevice::Init(params);

    AddCommand(scsi_command::cmd_test_unit_ready, [this]
        {
            TestUnitReady();
        });
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

    SetReady(true);

    return true;
}

void HostServices::TestUnitReady()
{
    // Always successful
    EnterStatusPhase();
}

vector<uint8_t> HostServices::InquiryInternal() const
{
    return HandleInquiry(device_type::processor, false);
}

void HostServices::StartStopUnit() const
{
    const bool start = GetController()->GetCdbByte(4) & 0x01;
    const bool load = GetController()->GetCdbByte(4) & 0x02;

    if (!start) {
        if (load) {
            GetController()->ScheduleShutdown(AbstractController::shutdown_mode::STOP_PI);
        }
        else {
            GetController()->ScheduleShutdown(AbstractController::shutdown_mode::STOP_S2P);
        }
    }
    else if (load) {
        GetController()->ScheduleShutdown(AbstractController::shutdown_mode::RESTART_PI);
    }
    else {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    EnterStatusPhase();
}

void HostServices::ExecuteOperation()
{
    execution_results.erase(GetController()->GetInitiatorId());

    input_format = ConvertFormat();

    const auto length = static_cast<size_t>(GetInt16(GetController()->GetCdb(), 7));
    if (!length) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    GetController()->SetLength(static_cast<uint32_t>(length));
    GetController()->SetByteTransfer(true);

    EnterDataOutPhase();
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

    const auto allocation_length = static_cast<size_t>(GetInt16(GetController()->GetCdb(), 7));
    const auto length = static_cast<int>(min(allocation_length, data.size()));
    if (!length) {
        EnterStatusPhase();
    }
    else {
        GetController()->CopyToBuffer(data.data(), length);

        EnterDataInPhase();
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

    // 4 bytes basic information
    const int size = AddModePages(cdb, buf, 4, length, 255);

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

    // 8 bytes basic information
    const int size = AddModePages(cdb, buf, 8, length, 65535);

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
    pages[32] = vector<byte>(10);

    if (!changeable) {
        const auto now = system_clock::now();
        const time_t t = system_clock::to_time_t(now);
        tm localtime;
        localtime_r(&t, &localtime);

        mode_page_datetime datetime;
        datetime.major_version = 0x01;
        datetime.minor_version = 0x00;
        datetime.year = (uint8_t)localtime.tm_year;
        datetime.month = (uint8_t)localtime.tm_mon;
        datetime.day = (uint8_t)localtime.tm_mday;
        datetime.hour = (uint8_t)localtime.tm_hour;
        datetime.minute = (uint8_t)localtime.tm_min;
        // Ignore leap second for simplicity
        datetime.second = (uint8_t)(localtime.tm_sec < 60 ? localtime.tm_sec : 59);

        memcpy(&pages[32][2], &datetime, sizeof(datetime));
    }
}

bool HostServices::WriteByteSequence(span<const uint8_t> buf)
{
    const auto length = GetInt16(GetController()->GetCdb(), 7);

    PbCommand command;
    switch (input_format) {
    case protobuf_format::binary:
        if (!command.ParseFromArray(buf.data(), length)) {
            LogTrace("Failed to deserialize protobuf binary data");
            return false;
        }
        break;

    case protobuf_format::json: {
        if (string cmd((const char*)buf.data(), length); !JsonStringToMessage(cmd, &command).ok()) {
            LogTrace("Failed to deserialize protobuf JSON data");
            return false;
        }
        break;
    }

    case protobuf_format::text: {
        if (string cmd((const char*)buf.data(), length); !TextFormat::ParseFromString(cmd, &command)) {
            LogTrace("Failed to deserialize protobuf text format data");
            return false;
        }
        break;
    }

    default:
        assert(false);
        break;
    }

    PbResult result;
    if (CommandContext context(command, s2p_image.GetDefaultFolder(), protobuf_util::GetParam(command, "locale"));
    !dispatcher->DispatchCommand(context, result, fmt::format("(ID:LUN {0}:{1}) - ", GetId(), GetLun()))) {
        LogTrace("Failed to execute " + PbOperation_Name(command.operation()) + " operation");
        return false;
    }

    execution_results[GetController()->GetInitiatorId()] = result.SerializeAsString();

    return true;
}

HostServices::protobuf_format HostServices::ConvertFormat() const
{
    switch (GetController()->GetCdbByte(1) & 0b00000111) {
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
