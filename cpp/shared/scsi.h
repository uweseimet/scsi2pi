//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <unordered_map>
#include <string>

using namespace std;

// Command Descriptor Block
using cdb_t = span<const int>;

namespace scsi_defs
{
enum class scsi_level
{
    none = 0,
    scsi_1_ccs = 1,
    scsi_2 = 2,
    spc = 3,
    spc_2 = 4,
    spc_3 = 5,
    spc_4 = 6,
    spc_5 = 7,
    spc_6 = 8
};

// Phase definitions
enum class phase_t
{
    busfree,
    arbitration,
    selection,
    reselection,
    command,
    datain,
    dataout,
    status,
    msgin,
    msgout,
    reserved
};

enum class device_type
{
    direct_access = 0,
    printer = 2,
    processor = 3,
    cd_rom = 5,
    optical_memory = 7,
    communications = 9
};

enum class scsi_command
{
    cmd_test_unit_ready = 0x00,
    cmd_rezero = 0x01,
    cmd_request_sense = 0x03,
    cmd_format_unit = 0x04,
    cmd_reassign_blocks = 0x07,
    cmd_read6 = 0x08,
    // DaynaPort specific command
    cmd_retrieve_stats = 0x09,
    cmd_write6 = 0x0a,
    cmd_print = 0x0a,
    cmd_seek6 = 0x0b,
    // DaynaPort specific command
    cmd_set_iface_mode = 0x0c,
    // DaynaPort specific command
    cmd_set_mcast_addr = 0x0d,
    // DaynaPort specific command
    cmd_enable_interface = 0x0e,
    cmd_synchronize_buffer = 0x10,
    cmd_inquiry = 0x12,
    cmd_mode_select6 = 0x15,
    cmd_reserve6 = 0x16,
    cmd_release6 = 0x17,
    cmd_mode_sense6 = 0x1a,
    cmd_start_stop = 0x1b,
    cmd_stop_print = 0x1b,
    cmd_send_diagnostic = 0x1d,
    cmd_prevent_allow_medium_removal = 0x1e,
    cmd_read_capacity10 = 0x25,
    cmd_read10 = 0x28,
    cmd_write10 = 0x2a,
    cmd_seek10 = 0x2b,
    cmd_verify10 = 0x2f,
    cmd_synchronize_cache10 = 0x35,
    cmd_read_defect_data10 = 0x37,
    cmd_read_long10 = 0x3e,
    cmd_write_long10 = 0x3f,
    cmd_read_toc = 0x43,
    cmd_mode_select10 = 0x55,
    cmd_mode_sense10 = 0x5a,
    cmd_read16 = 0x88,
    cmd_write16 = 0x8a,
    cmd_verify16 = 0x8f,
    cmd_synchronize_cache16 = 0x91,
    cmd_read_capacity16_read_long16 = 0x9e,
    cmd_write_long16 = 0x9f,
    cmd_report_luns = 0xa0,
    // Host services specific commands
    cmd_execute_operation = 0xc0,
    cmd_receive_operation_results = 0xc1
};

enum class status
{
    good = 0x00,
    check_condition = 0x02,
    condition_met = 0x04,
    busy = 0x08,
    intermediate = 0x10,
    intermediate_condition_met = 0x14,
    reservation_conflict = 0x18,
    command_terminated = 0x22,
    queue_full = 0x28
};

static const unordered_map<status, string> STATUS_MAPPING = {
    { status::good, "GOOD" },
    { status::check_condition, "CHECK CONDITION" },
    { status::condition_met, "CONDITION MET" },
    { status::busy, "CONDITION MET" },
    { status::intermediate, "INTERMEDIATE" },
    { status::condition_met, "CONDITION MET" },
    { status::intermediate_condition_met, "INTERMEDIATE-CONDITION MET" },
    { status::reservation_conflict, "RESERVATION CONFLICT" },
    { status::command_terminated, "COMMAND TERMINATED" },
    { status::queue_full, "QUEUE FULL" }
};

enum class sense_key
{
    no_sense = 0x00,
    recovered_error = 0x01,
    not_ready = 0x02,
    medium_error = 0x03,
    hardware_error = 0x04,
    illegal_request = 0x05,
    unit_attention = 0x06,
    data_protect = 0x07,
    blank_check = 0x08,
    vendor_specific = 0x09,
    copy_aborted = 0x0a,
    aborted_command = 0x0b,
    equal = 0x0c,
    volume_overflow = 0x0d,
    miscompare = 0x0e,
    reserved = 0x0f
};

static const unordered_map<sense_key, string> SENSE_KEY_MAPPING = {
    { sense_key::no_sense, "NO SENSE" },
    { sense_key::recovered_error, "RECOVERED ERROR" },
    { sense_key::not_ready, "NOT READY" },
    { sense_key::medium_error, "MEDIUM ERROR" },
    { sense_key::hardware_error, "HARDWARE ERROR" },
    { sense_key::illegal_request, "ILLEGAL REQUEST" },
    { sense_key::unit_attention, "UNIT ATTENTION" },
    { sense_key::data_protect, "DATA_PROTECT" },
    { sense_key::blank_check, "BLANK CHECK" },
    { sense_key::vendor_specific, "VENDOR SPECIFIC" },
    { sense_key::copy_aborted, "COPY ABORTED" },
    { sense_key::aborted_command, "ABORTED COMMAND" },
    { sense_key::equal, "EQUAL" },
    { sense_key::volume_overflow, "VOLUME OVERFLOW" },
    { sense_key::miscompare, "MISCOMPARE" },
    { sense_key::reserved, "RESERVED" }
};

enum class asc
{
    no_additional_sense_information = 0x00,
    write_fault = 0x03,
    read_fault = 0x11,
    parameter_list_length_error = 0x1a,
    invalid_command_operation_code = 0x20,
    lba_out_of_range = 0x21,
    invalid_field_in_cdb = 0x24,
    invalid_lun = 0x25,
    invalid_field_in_parameter_list = 0x26,
    write_protected = 0x27,
    not_ready_to_ready_change = 0x28,
    power_on_or_reset = 0x29,
    medium_not_present = 0x3a,
    command_phase_error = 0x4a,
    data_phase_error = 0x4b,
    load_or_eject_failed = 0x53,

    // SCSI2Pi-specific
    controller_process_phase = 0x80,
    controller_xfer_in = 0x88,
    controller_xfer_out = 0x89,
    daynaport_enable_interface = 0xf0,
    daynaport_disable_interface = 0xf1,
    printer_nothing_to_print = 0xf4,
    printer_printing_failed = 0xf5,
    printer_write_failed = 0xf6,
    host_services_receive_operation_results = 0xf8
};

// This map only contains mappings for ASCs used by s2p
static const unordered_map<asc, string> ASC_MAPPING = {
    { asc::no_additional_sense_information, "NO ADDITIONAL_SENSE INFORMATION" },
    { asc::write_fault, "WRITE FAULT" },
    { asc::read_fault, "READ ERROR" },
    { asc::parameter_list_length_error, "PARAMETER LIST LENGTH ERROR" },
    { asc::invalid_command_operation_code, "INVALID COMMAND OPERATION CODE" },
    { asc::lba_out_of_range, "LBA OUT OF RANGE" },
    { asc::invalid_field_in_cdb, "INVALID FIELD IN CDB" },
    { asc::invalid_lun, "LOGICAL UNIT NOT SUPPORTED" },
    { asc::invalid_field_in_parameter_list, "INVALID FIELD IN PARAMETER LIST" },
    { asc::write_protected, "WRITE PROTECTED" },
    { asc::not_ready_to_ready_change, "NOT READY TO READY TRANSITION (MEDIUM MAY HAVE CHANGED)" },
    { asc::power_on_or_reset, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED" },
    { asc::medium_not_present, "MEDIUM NOT PRESENT" },
    { asc::command_phase_error, "COMMAND PHASE ERROR" },
    { asc::data_phase_error, "DATA PHASE ERROR" },
    { asc::load_or_eject_failed, "MEDIA LOAD OR EJECT FAILED" }
};

// This map only contains mappings for commands supported by s2p
static const unordered_map<scsi_command, pair<int, string>> COMMAND_MAPPING = {
    { scsi_command::cmd_test_unit_ready, make_pair(6, "TEST UNIT READY") },
    { scsi_command::cmd_rezero, make_pair(6, "REZERO") },
    { scsi_command::cmd_request_sense, make_pair(6, "REQUEST SENSE") },
    { scsi_command::cmd_format_unit, make_pair(6, "FORMAT UNIT") },
    { scsi_command::cmd_reassign_blocks, make_pair(6, "REASSIGN BLOCKS") },
    { scsi_command::cmd_read6, make_pair(6, "READ(6)/GET MESSAGE(10)") },
    { scsi_command::cmd_retrieve_stats, make_pair(6, "RETRIEVE STATS") },
    { scsi_command::cmd_write6, make_pair(6, "WRITE(6)/PRINT/SEND MESSAGE(10)") },
    { scsi_command::cmd_seek6, make_pair(6, "SEEK(6)") },
    { scsi_command::cmd_set_iface_mode, make_pair(6, "SET IFACE MODE") },
    { scsi_command::cmd_set_mcast_addr, make_pair(6, "SET MCAST ADDR") },
    { scsi_command::cmd_enable_interface, make_pair(6, "ENABLE INTERFACE") },
    { scsi_command::cmd_synchronize_buffer, make_pair(6, "SYNCHRONIZE BUFFER") },
    { scsi_command::cmd_inquiry, make_pair(6, "INQUIRY") },
    { scsi_command::cmd_mode_select6, make_pair(6, "MODE SELECT(6)") },
    { scsi_command::cmd_reserve6, make_pair(6, "RESERVE(6)") },
    { scsi_command::cmd_release6, make_pair(6, "RELEASE(6)") },
    { scsi_command::cmd_mode_sense6, make_pair(6, "MODE SENSE(6)") },
    { scsi_command::cmd_start_stop, make_pair(6, "START STOP UNIT") },
    { scsi_command::cmd_stop_print, make_pair(6, "STOP PRINT") },
    { scsi_command::cmd_send_diagnostic, make_pair(6, "SEND DIAGNOSTIC") },
    { scsi_command::cmd_prevent_allow_medium_removal, make_pair(6, "PREVENT ALLOW MEDIUM REMOVAL") },
    { scsi_command::cmd_read_capacity10, make_pair(10, "READ CAPACITY(10)") },
    { scsi_command::cmd_read10, make_pair(10, "READ(10)") },
    { scsi_command::cmd_write10, make_pair(10, "WRITE(10)") },
    { scsi_command::cmd_seek10, make_pair(10, "SEEK(10)") },
    { scsi_command::cmd_verify10, make_pair(10, "VERIFY(10)") },
    { scsi_command::cmd_synchronize_cache10, make_pair(10, "SYNCHRONIZE CACHE(10)") },
    { scsi_command::cmd_read_defect_data10, make_pair(10, "READ DEFECT DATA(10)") },
    { scsi_command::cmd_read_long10, make_pair(10, "READ LONG(10)") },
    { scsi_command::cmd_write_long10, make_pair(10, "WRITE LONG(10)") },
    { scsi_command::cmd_read_toc, make_pair(10, "READ TOC") },
    { scsi_command::cmd_mode_select10, make_pair(10, "MODE SELECT(10)") },
    { scsi_command::cmd_mode_sense10, make_pair(10, "MODE SENSE(10)") },
    { scsi_command::cmd_read16, make_pair(16, "READ(16)") },
    { scsi_command::cmd_write16, make_pair(16, "WRITE(16)") },
    { scsi_command::cmd_verify16, make_pair(16, "VERIFY(16)") },
    { scsi_command::cmd_synchronize_cache16, make_pair(16, "SYNCHRONIZE CACHE(16") },
    { scsi_command::cmd_read_capacity16_read_long16, make_pair(16, "READ CAPACITY(16)/READ LONG(16)") },
    { scsi_command::cmd_write_long16, make_pair(16, "WRITE LONG(16)") },
    { scsi_command::cmd_report_luns, make_pair(12, "REPORT LUNS") },
    { scsi_command::cmd_execute_operation, make_pair(10, "EXECUTE OPERATION") },
    { scsi_command::cmd_receive_operation_results, make_pair(10, "RECEIVE OPERATION RESULTS") }
};
}
