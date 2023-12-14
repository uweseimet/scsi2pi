//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2021-2023 Uwe Seimet
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
    // Bridge specific command
    cmd_getMessage10 = 0x08,
    // DaynaPort specific command
    cmd_retrieve_stats = 0x09,
    cmd_write6 = 0x0a,
    // Bridge specific ommand
    cmd_sendMessage10 = 0x0a,
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
    cmd_get_event_status_notification = 0x4a,
    cmd_mode_select10 = 0x55,
    cmd_mode_sense10 = 0x5a,
    cmd_read16 = 0x88,
    cmd_write16 = 0x8a,
    cmd_verify16 = 0x8f,
    cmd_synchronize_cache16 = 0x91,
    cmd_read_capacity16_read_long16 = 0x9e,
    cmd_write_long16 = 0x9f,
    cmd_reportLuns = 0xa0,
    // Host services specific commands
    cmd_execute_operation = 0xc0,
    cmd_receive_operation_results = 0xc1
};

enum class status
{
    good = 0x00,
    check_condition = 0x02,
    reservation_conflict = 0x18
};

enum class sense_key
{
    no_sense = 0x00,
    not_ready = 0x02,
    medium_error = 0x03,
    illegal_request = 0x05,
    unit_attention = 0x06,
    data_protect = 0x07,
    aborted_command = 0x0b
};

enum class asc
{
    no_additional_sense_information = 0x00,
    write_fault = 0x03,
    read_fault = 0x11,
    invalid_command_operation_code = 0x20,
    lba_out_of_range = 0x21,
    invalid_field_in_cdb = 0x24,
    invalid_lun = 0x25,
    invalid_field_in_parameter_list = 0x26,
    write_protected = 0x27,
    not_ready_to_ready_change = 0x28,
    power_on_or_reset = 0x29,
    medium_not_present = 0x3a,
    load_or_eject_failed = 0x53
};

static const unordered_map<scsi_command, pair<int, string>> command_mapping = {
    { scsi_command::cmd_test_unit_ready, make_pair(6, "TestUnitReady") },
    { scsi_command::cmd_rezero, make_pair(6, "Rezero") },
    { scsi_command::cmd_request_sense, make_pair(6, "RequestSense") },
    { scsi_command::cmd_format_unit, make_pair(6, "FormatUnit") },
    { scsi_command::cmd_reassign_blocks, make_pair(6, "ReassignBlocks") },
    { scsi_command::cmd_read6, make_pair(6, "Read6/GetMessage10") },
    { scsi_command::cmd_retrieve_stats, make_pair(6, "RetrieveStats") },
    { scsi_command::cmd_write6, make_pair(6, "Write6/Print/SendMessage10") },
    { scsi_command::cmd_seek6, make_pair(6, "Seek6") },
    { scsi_command::cmd_set_iface_mode, make_pair(6, "SetIfaceMode") },
    { scsi_command::cmd_set_mcast_addr, make_pair(6, "SetMcastAddr") },
    { scsi_command::cmd_enable_interface, make_pair(6, "EnableInterface") },
    { scsi_command::cmd_synchronize_buffer, make_pair(6, "Synchronize_buffer") },
    { scsi_command::cmd_inquiry, make_pair(6, "Inquiry") },
    { scsi_command::cmd_mode_select6, make_pair(6, "ModeSelect6") },
    { scsi_command::cmd_reserve6, make_pair(6, "Reserve6") },
    { scsi_command::cmd_release6, make_pair(6, "Release6") },
    { scsi_command::cmd_mode_sense6, make_pair(6, "ModeSense6") },
    { scsi_command::cmd_start_stop, make_pair(6, "StartStop") },
    { scsi_command::cmd_stop_print, make_pair(6, "StopPrint") },
    { scsi_command::cmd_send_diagnostic, make_pair(6, "SendDiagnostic") },
    { scsi_command::cmd_prevent_allow_medium_removal, make_pair(6, "PreventAllowMediumRemoval") },
    { scsi_command::cmd_read_capacity10, make_pair(10, "ReadCapacity10") },
    { scsi_command::cmd_read10, make_pair(10, "Read10") },
    { scsi_command::cmd_write10, make_pair(10, "Write10") },
    { scsi_command::cmd_seek10, make_pair(10, "Seek10") },
    { scsi_command::cmd_verify10, make_pair(10, "Verify10") },
    { scsi_command::cmd_synchronize_cache10, make_pair(10, "SynchronizeCache10") },
    { scsi_command::cmd_read_defect_data10, make_pair(10, "ReadDefectData10") },
    { scsi_command::cmd_read_long10, make_pair(10, "ReadLong10") },
    { scsi_command::cmd_write_long10, make_pair(10, "WriteLong10") },
    { scsi_command::cmd_read_toc, make_pair(10, "ReadToc") },
    { scsi_command::cmd_get_event_status_notification, make_pair(10, "GetEventStatusNotification") },
    { scsi_command::cmd_mode_select10, make_pair(10, "ModeSelect10") },
    { scsi_command::cmd_mode_sense10, make_pair(10, "ModeSense10") },
    { scsi_command::cmd_read16, make_pair(16, "Read16") },
    { scsi_command::cmd_write16, make_pair(16, "Write16") },
    { scsi_command::cmd_verify16, make_pair(16, "Verify16") },
    { scsi_command::cmd_synchronize_cache16, make_pair(16, "SynchronizeCache16") },
    { scsi_command::cmd_read_capacity16_read_long16, make_pair(16, "ReadCapacity16/ReadLong16") },
    { scsi_command::cmd_write_long16, make_pair(16, "WriteLong16") },
    { scsi_command::cmd_reportLuns, make_pair(12, "ReportLuns") },
    { scsi_command::cmd_execute_operation, make_pair(10, "ExecuteOperation") },
    { scsi_command::cmd_receive_operation_results, make_pair(10, "Receive_operation_results") }
};
}
