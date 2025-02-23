//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
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
#include "controllers/abstract_controller.h"
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_exceptions.h"
#include "page_handler.h"

using namespace std::chrono;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace memory_util;
using namespace s2p_interface_util;

HostServices::HostServices(int lun) : PrimaryDevice(SCHS, lun)
{
    PrimaryDevice::SetProductData( { "", "Host Services", "" }, true);
    SetScsiLevel(ScsiLevel::SPC_3);
    SetReady(true);
}

string HostServices::SetUp()
{
    AddCommand(ScsiCommand::START_STOP, [this]
        {
            StartStopUnit();
        });
    AddCommand(ScsiCommand::EXECUTE_OPERATION, [this]
        {
            ExecuteOperation();
        });
    AddCommand(ScsiCommand::RECEIVE_OPERATION_RESULTS, [this]
        {
            ReceiveOperationResults();
        });

    page_handler = make_unique<PageHandler>(*this, false, false);

    return "";
}

vector<uint8_t> HostServices::InquiryInternal() const
{
    return HandleInquiry(DeviceType::PROCESSOR, false);
}

void HostServices::StartStopUnit() const
{
    const bool load = GetCdbByte(4) & 0x02;

    if (const bool start = GetCdbByte(4) & 0x01; !start) {
        GetController()->ScheduleShutdown(load ? ShutdownMode::STOP_PI : ShutdownMode::STOP_S2P);
    }
    else if (load) {
        GetController()->ScheduleShutdown(ShutdownMode::RESTART_PI);
    }
    else {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    StatusPhase();
}

void HostServices::ExecuteOperation()
{
    execution_results.erase(GetController()->GetInitiatorId());

    input_format = ConvertFormat();

    const int length = GetCdbInt16(7);
    if (!length) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    GetController()->SetTransferSize(length, length);

    DataOutPhase(length);
}

void HostServices::ReceiveOperationResults()
{
    const ProtobufFormat output_format = ConvertFormat();

    const auto &it = execution_results.find(GetController()->GetInitiatorId());
    if (it == execution_results.end()) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::DATA_CURRENTLY_UNAVAILABLE);
    }
    const string &execution_result = it->second;

    string data;
    switch (output_format) {
    case ProtobufFormat::BINARY:
        data = execution_result;
        break;

    case ProtobufFormat::JSON: {
        PbResult result;
        result.ParseFromArray(execution_result.data(), static_cast<int>(execution_result.size()));
        (void)MessageToJsonString(result, &data).ok();
        break;
    }

    case ProtobufFormat::TEXT: {
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

    const int length = min(GetCdbInt16(7), static_cast<int>(data.size()));
    GetController()->CopyToBuffer(data.data(), length);

    DataInPhase(length);
}

int HostServices::ModeSense6(cdb_t cdb, data_in_t buf) const
{
    // Block descriptors cannot be returned, subpages are not supported
    if (cdb[3] || !(cdb[1] & 0x08)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const int length = min(static_cast<int>(buf.size()), cdb[4]);
    fill_n(buf.begin(), length, 0);

    const int size = page_handler->AddModePages(cdb, buf, 4, length, 255);

    // The size field does not count itself
    buf[0] = static_cast<uint8_t>((size - 1));

    return size;
}

int HostServices::ModeSense10(cdb_t cdb, data_in_t buf) const
{
    // Block descriptors cannot be returned, subpages are not supported
    if (cdb[3] || !(cdb[1] & 0x08)) {
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    const int length = min(static_cast<int>(buf.size()), GetInt16(cdb, 7));
    fill_n(buf.begin(), length, 0);

    const int size = page_handler->AddModePages(cdb, buf, 8, length, 65535);

    // The size field does not count itself
    SetInt16(buf, 0, size - 2);

    return size;
}

void HostServices::SetUpModePages(map<int, vector<byte>> &pages, int page, bool changeable) const
{
    if (page == 0x20 || page == 0x3f) {
        AddRealtimeClockPage(pages, changeable);
    }
}

void HostServices::AddRealtimeClockPage(map<int, vector<byte>> &pages, bool changeable)
{
    pages[32] = vector<byte>(sizeof(ModePageDateTime) + 2);

    if (!changeable) {
        const time_t &t = system_clock::to_time_t(system_clock::now());
        tm local_time;
        localtime_r(&t, &local_time);

        ModePageDateTime datetime;
        datetime.major_version = 0x01;
        datetime.minor_version = 0x00;
        datetime.year = static_cast<uint8_t>(local_time.tm_year);
        datetime.month = static_cast<uint8_t>(local_time.tm_mon);
        datetime.day = static_cast<uint8_t>(local_time.tm_mday);
        datetime.hour = static_cast<uint8_t>(local_time.tm_hour);
        datetime.minute = static_cast<uint8_t>(local_time.tm_min);
        // Ignore leap second for simplicity
        datetime.second = static_cast<uint8_t>(local_time.tm_sec < 60 ? local_time.tm_sec : 59);

        memcpy(&pages[32][2], &datetime, sizeof(datetime));
    }
}

int HostServices::WriteData(cdb_t cdb, data_out_t buf, int, int l)
{
    if (static_cast<ScsiCommand>(cdb[0]) != ScsiCommand::EXECUTE_OPERATION) {
        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
    }

    const auto length = GetCdbInt16(7);
    if (!length) {
        execution_results[GetController()->GetInitiatorId()].clear();
        return l;
    }

    PbCommand cmd;
    switch (input_format) {
    case ProtobufFormat::BINARY:
        if (!cmd.ParseFromArray(buf.data(), length)) {
            throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
        }
        break;

    case ProtobufFormat::JSON:
        if (string c((const char*)buf.data(), length); !JsonStringToMessage(c, &cmd).ok()) {
            throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
        }
        break;

    case ProtobufFormat::TEXT:
        if (string c((const char*)buf.data(), length); !TextFormat::ParseFromString(c, &cmd)) {
            throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
        }
        break;

    default:
        assert(false);
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    PbResult result;
    CommandContext context(cmd, GetLogger());
    context.SetLocale(s2p_interface_util::GetParam(cmd, "locale"));
    if (!dispatcher->DispatchCommand(context, result)) {
        LogTrace("Failed to execute " + PbOperation_Name(cmd.operation()) + " operation");
        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
    }

    execution_results[GetController()->GetInitiatorId()] = result.SerializeAsString();

    return l;
}

ProtobufFormat HostServices::ConvertFormat() const
{
    switch (GetCdbByte(1) & 0b00011111) {
    case 0x001:
        return ProtobufFormat::BINARY;

    case 0b010:
        return ProtobufFormat::JSON;

    case 0b100:
        return ProtobufFormat::TEXT;

    default:
        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }
}
