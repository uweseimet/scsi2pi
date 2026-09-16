//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <span>
#include <vector>
#include "scsi.h"

using namespace std;
using namespace filesystem;
using enum StatusCode;
using enum SenseKey;
using enum Asc;

namespace s2p_util
{

// Separator for compound options like ID:LUN
inline constexpr char COMPONENT_SEPARATOR = ':';

struct StringHash
{
    using is_transparent = void;

    size_t operator()(string_view s) const
    {
        return hash<string_view> { }(s);
    }
};

inline string Join(const auto &collection, const string &separator = ", ")
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
bool IsReadOnlyFile(const path&);
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
string FormatSenseData(SenseKey, Asc, uint8_t = 0);

vector<byte> HexToBytes(const string&);

constexpr int HexToDec(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

string_view Trim(string_view);

void Sleep(const timespec&);

off_t GetCapacityFromFile(const string&);

using SignalHandlerPtr = void(*)(int);
void SetTerminationHandler(SignalHandlerPtr);

constexpr const char* to_const_char_ptr(span<const uint8_t> bytes)
{
    return static_cast<const char*>(static_cast<const void*>(bytes.data()));
}

constexpr char* to_char_ptr(span<uint8_t> bytes)
{
    return static_cast<char*>(static_cast<void*>(bytes.data()));
}

constexpr const char* to_const_char_ptr(span<byte> bytes)
{
    return static_cast<const char*>(static_cast<const void*>(bytes.data()));
}

constexpr char* to_char_ptr(span<byte> bytes)
{
    return static_cast<char*>(static_cast<void*>(bytes.data()));
}

inline constexpr array<const char*, 16> SENSE_KEYS = {
    "NO SENSE",
    "RECOVERED ERROR",
    "NOT READY",
    "MEDIUM ERROR",
    "HARDWARE ERROR",
    "ILLEGAL REQUEST",
    "UNIT ATTENTION",
    "DATA PROTECT",
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
inline const unordered_map<Asc, const char*> ASC_MAPPING = {
    { NO_ADDITIONAL_SENSE_INFORMATION, "NO ADDITIONAL SENSE INFORMATION" },
    { WRITE_FAULT, "PERIPHERAL DEVICE WRITE FAULT" },
    { IO_PROCESS_TERMINATED, "I/O PROCESS TERMINATED" },
    { WRITE_ERROR, "WRITE ERROR" },
    { READ_ERROR, "READ ERROR" },
    { LOCATE_OPERATION_FAILURE, "LOCATE OPERATION FAILURE" },
    { PARAMETER_LIST_LENGTH_ERROR, "PARAMETER LIST LENGTH ERROR" },
    { INVALID_COMMAND_OPERATION_CODE, "INVALID COMMAND OPERATION CODE" },
    { LBA_OUT_OF_RANGE, "LBA OUT OF RANGE" },
    { INVALID_FIELD_IN_CDB, "INVALID FIELD IN CDB" },
    { LOGICAL_UNIT_NOT_SUPPORTED, "LOGICAL UNIT NOT SUPPORTED" },
    { INVALID_FIELD_IN_PARAMETER_LIST, "INVALID FIELD IN PARAMETER LIST" },
    { WRITE_PROTECTED, "WRITE PROTECTED" },
    { NOT_READY_TO_READY_TRANSITION, "NOT READY TO READY TRANSITION (MEDIUM MAY HAVE CHANGED)" },
    { POWER_ON_OR_RESET, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED" },
    { INCOMPATIBLE_MEDIUM_INSTALLED, "INCOMPATIBLE MEDIUM INSTALLED" },
    { SEQUENTIAL_POSITIONING_ERROR, "SEQUENTIAL POSITIONING ERROR" },
    { MEDIUM_NOT_PRESENT, "MEDIUM NOT PRESENT" },
    { INTERNAL_TARGET_FAILURE, "INTERNAL TARGET FAILURE" },
    { COMMAND_PHASE_ERROR, "COMMAND PHASE ERROR" },
    { DATA_PHASE_ERROR, "DATA PHASE ERROR" },
    { MEDIA_LOAD_OR_EJECT_FAILED, "MEDIA LOAD OR EJECT FAILED" },
    { DATA_CURRENTLY_UNAVAILABLE, "DATA CURRENTLY UNAVAILABLE" }
};

inline const unordered_map<StatusCode, const char*> STATUS_MAPPING = {
    { GOOD, "GOOD" },
    { CHECK_CONDITION, "CHECK CONDITION" },
    { CONDITION_MET, "CONDITION MET" },
    { BUSY, "BUSY" },
    { INTERMEDIATE, "INTERMEDIATE" },
    { INTERMEDIATE_CONDITION_MET, "INTERMEDIATE-CONDITION MET" },
    { RESERVATION_CONFLICT, "RESERVATION CONFLICT" },
    { COMMAND_TERMINATED, "COMMAND TERMINATED" },
    { QUEUE_FULL, "QUEUE FULL" },
    { ACA_ACTIVE, "ACA ACTIVE" },
    { TASK_ABORTED, "TASK ABORTED" }
};
}
