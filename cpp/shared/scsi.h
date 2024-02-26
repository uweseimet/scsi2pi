//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <unordered_map>
#include <string>

using namespace std;

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

enum class phase_t
{
    busfree = 0,
    arbitration = 1,
    selection = 2,
    reselection = 3,
    command = 4,
    datain = 5,
    dataout = 6,
    status = 7,
    msgin = 8,
    msgout = 9,
    reserved = 10
};

enum class device_type
{
    direct_access = 0,
    printer = 2,
    processor = 3,
    cd_rom = 5,
    optical_memory = 7
};

enum class scsi_command
{
    cmd_test_unit_ready = 0x00,
    cmd_rezero = 0x01,
    cmd_request_sense = 0x03,
    cmd_format_unit = 0x04,
    cmd_reassign_blocks = 0x07,
    cmd_read6 = 0x08,
    cmd_get_message6 = 0x08,
    // DaynaPort specific command
    cmd_retrieve_stats = 0x09,
    cmd_write6 = 0x0a,
    cmd_send_message6 = 0x0a,
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

static constexpr array<string, 16> SENSE_KEYS = {
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
}
