//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

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
    spc_6 = 8,
    last = 9
};

enum class bus_phase
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
    sequential_access = 1,
    printer = 2,
    processor = 3,
    cd_rom = 5,
    optical_memory = 7
};

enum class scsi_command
{
    test_unit_ready = 0x00,
    rezero = 0x01,
    rewind = 0x01,
    request_sense = 0x03,
    format_unit = 0x04,
    format_medium = 0x04,
    read_block_limits = 0x05,
    reassign_blocks = 0x07,
    read_6 = 0x08,
    get_message_6 = 0x08,
    // DaynaPort-specific command
    retrieve_stats = 0x09,
    write_6 = 0x0a,
    send_message_6 = 0x0a,
    print = 0x0a,
    seek_6 = 0x0b,
    // DaynaPort-specific command
    set_iface_mode = 0x0c,
    // DaynaPort-specific command
    set_mcast_addr = 0x0d,
    // DaynaPort-specific command
    enable_interface = 0x0e,
    synchronize_buffer = 0x10,
    write_filemarks_6 = 0x10,
    space_6 = 0x11,
    inquiry = 0x12,
    mode_select_6 = 0x15,
    reserve_6 = 0x16,
    release_6 = 0x17,
    erase_6 = 0x19,
    mode_sense_6 = 0x1a,
    start_stop = 0x1b,
    stop_print = 0x1b,
    send_diagnostic = 0x1d,
    prevent_allow_medium_removal = 0x1e,
    read_format_capacities = 0x23,
    read_capacity_10 = 0x25,
    read_10 = 0x28,
    write_10 = 0x2a,
    seek_10 = 0x2b,
    locate_10 = 0x2b,
    verify_10 = 0x2f,
    read_position = 0x34,
    synchronize_cache_10 = 0x35,
    read_defect_data_10 = 0x37,
    read_long_10 = 0x3e,
    write_long_10 = 0x3f,
    read_toc = 0x43,
    mode_select_10 = 0x55,
    mode_sense_10 = 0x5a,
    read_16 = 0x88,
    write_16 = 0x8a,
    verify_16 = 0x8f,
    synchronize_cache_16 = 0x91,
    locate_16 = 0x92,
    read_capacity_16_read_long_16 = 0x9e,
    write_long_16 = 0x9f,
    report_luns = 0xa0,
    // SCSi2Pi-specific commands (host services)
    execute_operation = 0xc0,
    receive_operation_results = 0xc1
};

enum class message_code
{
    command_complete = 0x00,
    abort = 0x06,
    message_reject = 0x07,
    linked_command_complete = 0x0a,
    linked_command_complete_with_flag = 0x0b,
    bus_device_reset = 0x0c,
    identify = 0x80
};

enum class status_code
{
    good = 0x00,
    check_condition = 0x02,
    condition_met = 0x04,
    busy = 0x08,
    intermediate = 0x10,
    intermediate_condition_met = 0x14,
    reservation_conflict = 0x18,
    command_terminated = 0x22,
    queue_full = 0x28,
    aca_active = 0x30,
    task_aborted = 0x40
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

enum class asc
{
    no_additional_sense_information = 0x00,
    write_fault = 0x03,
    write_error = 0x0c,
    read_error = 0x11,
    parameter_list_length_error = 0x1a,
    invalid_command_operation_code = 0x20,
    lba_out_of_range = 0x21,
    invalid_field_in_cdb = 0x24,
    invalid_lun = 0x25,
    invalid_field_in_parameter_list = 0x26,
    write_protected = 0x27,
    not_ready_to_ready_change = 0x28,
    power_on_or_reset = 0x29,
    sequential_positioning_error = 0x38,
    medium_not_present = 0x3a,
    command_phase_error = 0x4a,
    data_phase_error = 0x4b,
    medium_load_or_eject_failed = 0x53,

    // SCSI2Pi-specific
    controller_process_phase = 0x80,
    daynaport_enable_interface = 0xf0,
    daynaport_disable_interface = 0xf1,
    printer_nothing_to_print = 0xf4,
    printer_printing_failed = 0xf5,
    printer_write_failed = 0xf6,
    host_services_receive_operation_results = 0xf8
};

enum class ascq
{
    none = 0x00,
    filemark_detected = 0x01,
    end_of_partition_medium_detected = 0x02,
    beginning_of_partition_medium_detected = 0x04,
    end_of_data_detected = 0x05,
};
