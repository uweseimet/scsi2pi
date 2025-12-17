//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <span>
#include <vector>
#include <spdlog/spdlog.h>
#include "scsi.h"

using namespace std;

namespace s2p_util
{

// Separator for compound options like ID:LUN
static constexpr char COMPONENT_SEPARATOR = ':';

struct StringHash
{
    using is_transparent = void;

    size_t operator()(string_view sv) const
    {
        hash<string_view> hasher;
        return hasher(sv);
    }
};

string Join(const auto &collection, const string &separator = ", ")
{
    // Using a stream (and not a string) is required in order to correctly convert the element data
    ostringstream s;

    for (const auto &element : collection) {
        if (s.tellp()) {
            s << separator;
        }

        s << element;
    }

    return s.str();
}

string GetVersionString();
string GetHomeDir();
pair<int, int> GetUidAndGid();
vector<string> Split(const string&, char, int = numeric_limits<int>::max());
string ToUpper(string_view);
string ToLower(string_view);
string GetExtensionLowerCase(string_view);
string GetLocale();
string GetLine(const string&, istream& = cin);
int ParseAsUnsignedInt(const string&);
string ParseIdAndLun(const string&, int&, int&);
string Banner(string_view);

tuple<string, string, string> GetInquiryProductData(span<const uint8_t>);

string GetScsiLevel(int);

string GetStatusString(int);

string FormatSenseData(span<const byte>);
string FormatSenseData(SenseKey, Asc, int = 0);

vector<byte> HexToBytes(const string&);
int HexToDec(char);

string_view Trim(string_view);

void Sleep(const timespec&);

shared_ptr<spdlog::logger> CreateLogger(const string&);

static constexpr array<const char*, 16> SENSE_KEYS = {
    "NO SENSE",
    "RECOVERED ERROR",
    "NOT READY",
    "MEDIUM ERROR",
    "HARDWARE ERROR",
    "ILLEGAL REQUEST",
    "UNIT ATTENTION",
    "DATA_PROTECT",
    "BLANK CHECK",
    "VENDOR SPECIFIC",
    "COPY ABORTED",
    "ABORTED COMMAND",
    "EQUAL",
    "VOLUME OVERFLOW",
    "MISCOMPARE",
    "RESERVED"
};

// This map only contains mappings for ASCs used by s2p or the Linux SG driver
static const unordered_map<Asc, const char*> ASC_MAPPING = {
    { Asc::NO_ADDITIONAL_SENSE_INFORMATION, "NO ADDITIONAL SENSE INFORMATION" },
    { Asc::WRITE_FAULT, "PERIPHERAL DEVICE WRITE FAULT" },
    { Asc::IO_PROCESS_TERMINATED, "I/O PROCESS TERMINATED" },
    { Asc::WRITE_ERROR, "WRITE ERROR" },
    { Asc::READ_ERROR, "READ ERROR" },
    { Asc::LOCATE_OPERATION_FAILURE, "LOCATE OPERATION FAILURE" },
    { Asc::PARAMETER_LIST_LENGTH_ERROR, "PARAMETER LIST LENGTH ERROR" },
    { Asc::INVALID_COMMAND_OPERATION_CODE, "INVALID COMMAND OPERATION CODE" },
    { Asc::LBA_OUT_OF_RANGE, "LBA OUT OF RANGE" },
    { Asc::INVALID_FIELD_IN_CDB, "INVALID FIELD IN CDB" },
    { Asc::LOGICAL_UNIT_NOT_SUPPORTED, "LOGICAL UNIT NOT SUPPORTED" },
    { Asc::INVALID_FIELD_IN_PARAMETER_LIST, "INVALID FIELD IN PARAMETER LIST" },
    { Asc::WRITE_PROTECTED, "WRITE PROTECTED" },
    { Asc::NOT_READY_TO_READY_TRANSITION, "NOT READY TO READY TRANSITION (MEDIUM MAY HAVE CHANGED)" },
    { Asc::POWER_ON_OR_RESET, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED" },
    { Asc::INCOMPATIBLE_MEDIUM_INSTALLED, "INCOMPATIBLE MEDIUM INSTALLED" },
    { Asc::SEQUENTIAL_POSITIONING_ERROR, "SEQUENTIAL POSITIONING ERROR" },
    { Asc::MEDIUM_NOT_PRESENT, "MEDIUM NOT PRESENT" },
    { Asc::INTERNAL_TARGET_FAILURE, "INTERNAL TARGET FAILURE" },
    { Asc::COMMAND_PHASE_ERROR, "COMMAND PHASE ERROR" },
    { Asc::DATA_PHASE_ERROR, "DATA PHASE ERROR" },
    { Asc::MEDIUM_LOAD_OR_EJECT_FAILED, "MEDIA LOAD OR EJECT FAILED" },
    { Asc::DATA_CURRENTLY_UNAVAILABLE, "DATA CURRENTLY UNAVAILALBLE" }
};

static const unordered_map<StatusCode, const char*> STATUS_MAPPING = {
    { StatusCode::GOOD, "GOOD" },
    { StatusCode::CHECK_CONDITION, "CHECK CONDITION" },
    { StatusCode::CONDITION_MET, "CONDITION MET" },
    { StatusCode::BUSY, "BUSY" },
    { StatusCode::INTERMEDIATE, "INTERMEDIATE" },
    { StatusCode::INTERMEDIATE_CONDITION_MET, "INTERMEDIATE-CONDITION MET" },
    { StatusCode::RESERVATION_CONFLICT, "RESERVATION CONFLICT" },
    { StatusCode::COMMAND_TERMINATED, "COMMAND TERMINATED" },
    { StatusCode::QUEUE_FULL, "QUEUE FULL" },
    { StatusCode::ACA_ACTIVE, "ACA ACTIVE" },
    { StatusCode::TASK_ABORTED, "TASK ABORTED" }
};
}
