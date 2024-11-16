//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>
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

string Join(const auto &collection, const char *separator = ", ")
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
string ToUpper(const string&);
string ToLower(const string&);
string GetExtensionLowerCase(string_view);
string GetLocale();
string GetLine(const string&);
bool GetAsUnsignedInt(const string&, int&);
string ProcessId(const string&, int&, int&);
string Banner(string_view);

string GetScsiLevel(int);

string FormatSenseData(sense_key, asc, int = 0);

vector<byte> HexToBytes(const string&);
string FormatBytes(vector<uint8_t>&, int, bool = false);
int HexToDec(char);

string Trim(const string&);

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

// This map only contains mappings for ASCs used by s2p
static const unordered_map<asc, const char*> ASC_MAPPING = {
    { asc::no_additional_sense_information, "NO ADDITIONAL_SENSE INFORMATION" },
    { asc::write_fault, "PERIPHERAL DEVICE WRITE FAULT" },
    { asc::write_error, "WRITE ERROR" },
    { asc::read_error, "READ ERROR" },
    { asc::parameter_list_length_error, "PARAMETER LIST LENGTH ERROR" },
    { asc::invalid_command_operation_code, "INVALID COMMAND OPERATION CODE" },
    { asc::lba_out_of_range, "LBA OUT OF RANGE" },
    { asc::invalid_field_in_cdb, "INVALID FIELD IN CDB" },
    { asc::invalid_lun, "LOGICAL UNIT NOT SUPPORTED" },
    { asc::invalid_field_in_parameter_list, "INVALID FIELD IN PARAMETER LIST" },
    { asc::write_protected, "WRITE PROTECTED" },
    { asc::not_ready_to_ready_change, "NOT READY TO READY TRANSITION (MEDIUM MAY HAVE CHANGED)" },
    { asc::power_on_or_reset, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED" },
    { asc::sequential_positioning_error, "SEQUENTIAL POSITIONING ERROR" },
    { asc::medium_not_present, "MEDIUM NOT PRESENT" },
    { asc::command_phase_error, "COMMAND PHASE ERROR" },
    { asc::data_phase_error, "DATA PHASE ERROR" },
    { asc::load_or_eject_failed, "MEDIA LOAD OR EJECT FAILED" }
};

static const unordered_map<status_code, const char*> STATUS_MAPPING = {
    { status_code::good, "GOOD" },
    { status_code::check_condition, "CHECK CONDITION" },
    { status_code::condition_met, "CONDITION MET" },
    { status_code::busy, "BUSY" },
    { status_code::intermediate, "INTERMEDIATE" },
    { status_code::intermediate_condition_met, "INTERMEDIATE-CONDITION MET" },
    { status_code::reservation_conflict, "RESERVATION CONFLICT" },
    { status_code::command_terminated, "COMMAND TERMINATED" },
    { status_code::queue_full, "QUEUE FULL" },
    { status_code::aca_active, "ACA ACTIVE" },
    { status_code::task_aborted, "TASK ABORTED" }
};
}
