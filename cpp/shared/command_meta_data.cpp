//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include <spdlog/spdlog.h>
#include "command_meta_data.h"

using namespace spdlog;

CommandMetaData::CommandMetaData()
{
    for (int i = 0x00; i < 0x1f; i++) {
        AddCommand(static_cast<scsi_command>(i), 6, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0x20; i < 0x7f; i++) {
        AddCommand(static_cast<scsi_command>(i), 10, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0x80; i < 0xa0; i++) {
        AddCommand(static_cast<scsi_command>(i), 16, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0xa0; i < 0xc0; i++) {
        AddCommand(static_cast<scsi_command>(i), 12, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    // This explicit mapping contains all commands supported by s2p (see https://www.scsi2pi.net/en/scsi_commands.html)
    // and some others typically used with the SCSG device
    AddCommand(scsi_command::test_unit_ready, 6, "TEST UNIT READY", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::rezero, 6, "REZERO/REWIND", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_block_limits, 6, "READ BLOCK LIMITS", { -6, 0, 0, 0, false });
    AddCommand(scsi_command::request_sense, 6, "REQUEST SENSE", { 4, 1, 0, 0, false });
    AddCommand(scsi_command::format_unit, 6, "FORMAT UNIT/FORMAT MEDIUM", { 0, 0, 0, 0, true });
    AddCommand(scsi_command::reassign_blocks, 6, "REASSIGN BLOCKS", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_6, 6, "READ(6)/GET MESSAGE(6)", { 4, 1, 1, 3, false });
    AddCommand(scsi_command::retrieve_stats, 6, "RETRIEVE STATS", { 4, 1, 0, 0, false });
    AddCommand(scsi_command::write_6, 6, "WRITE(6)/SEND MESSAGE(6)/PRINT", { 4, 1, 1, 3, true });
    AddCommand(scsi_command::seek_6, 6, "SEEK(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::set_iface_mode, 6, "SET INTERFACE MODE", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::set_mcast_addr, 6, "SET MULTICAST ADDRESS", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::enable_interface, 6, "ENABLE INTERFACE", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::synchronize_buffer, 6, "SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::space_6, 6, "SPACE(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::inquiry, 6, "INQUIRY", { 4, 1, 0, 0, false });
    AddCommand(scsi_command::mode_select_6, 6, "MODE SELECT(6)", { 4, 1, 0, 0, true });
    AddCommand(scsi_command::reserve_6, 6, "RESERVE(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::release_6, 6, "RELEASE(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::erase_6, 6, "ERASE(6)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::mode_sense_6, 6, "MODE SENSE(6)", { 4, 1, 0, 0, false });
    AddCommand(scsi_command::start_stop, 6, "START STOP UNIT/STOP PRINT", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::send_diagnostic, 6, "SEND DIAGNOSTIC", { 3, 2, 0, 0, false });
    AddCommand(scsi_command::prevent_allow_medium_removal, 6, "PREVENT ALLOW MEDIUM REMOVAL", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_format_capacities, 10, "READ FORMAT CAPACITIES", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_capacity_10, 10, "READ CAPACITY(10)", { -8, 0, 0, 0, false });
    AddCommand(scsi_command::read_10, 10, "READ(10)", { 7, 2, 2, 4, false });
    AddCommand(scsi_command::write_10, 10, "WRITE(10)", { 7, 2, 2, 4, true });
    AddCommand(scsi_command::seek_10, 10, "SEEK(10)/LOCATE(10)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::verify_10, 10, "VERIFY(10)", { 7, 2, 2, 4, true });
    AddCommand(scsi_command::synchronize_cache_10, 10, "SYNCHRONIZE CACHE(10)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_defect_data_10, 10, "READ DEFECT DATA(10)", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_long_10, 10, "READ LONG(10)", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::write_long_10, 10, "WRITE LONG(10)", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_sub_channel, 10, "READ SUB-CHANNEL", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_toc, 10, "READ TOC", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_header, 10, "READ HEADER", { 7, 2, 2, 4, false });
    AddCommand(scsi_command::play_audio_10, 10, "PLAY AUDIO(10)", { 7, 2, 2, 4, false });
    AddCommand(scsi_command::get_configuration, 10, "GET CONFIGURATION", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::play_audio_msf, 10, "PLAY AUDIO MSF", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::play_audio_track_index, 10, "PLAY AUDIO TRACK/INDEX", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::get_event_status_notification, 10, "GET EVENT/STATUS NOTIFICATION", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::pause_resume, 10, "PAUSE/RESUME", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::log_select, 10, "LOG SELECT", { 7, 2, 0, 0, true });
    AddCommand(scsi_command::log_sense, 10, "LOG SENSE", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_disc_information, 10, "READ DISC INFORMATION", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::read_track_information, 10, "READ TRACk INFORMATION", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::mode_select_10, 10, "MODE SELECT(10)", { 7, 2, 0, 0, true });
    AddCommand(scsi_command::read_master_cue, 10, "READ MASTER CUE", { 6, 3, 0, 0, false });
    AddCommand(scsi_command::mode_sense_10, 10, "MODE SENSE(10)", { 7, 2, 0, 0, false });
    AddCommand(scsi_command::close_track_session, 10, "CLOSE TRACK/SESSION", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::write_filemarks_16, 16, "WRITE FILEMARKS(16)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_16, 16, "READ(16)", { 10, 4, 2, 8, false });
    AddCommand(scsi_command::write_16, 16, "WRITE(16)", { 10, 4, 2, 8, true });
    AddCommand(scsi_command::verify_16, 16, "VERIFY(16)", { 10, 4, 2, 8, true });
    AddCommand(scsi_command::read_position, 10, "READ POSITION", { 7, 2, 0, 0 });
    AddCommand(scsi_command::synchronize_cache_16, 16, "SYNCHRONIZE CACHE(16)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::locate_16, 16, "LOCATE(16)", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::read_capacity_16_read_long_16, 16, "READ CAPACITY(16)/READ LONG(16)",
        { 12, 2, 0, 0, false });
    AddCommand(scsi_command::write_long_16, 16, "WRITE LONG(16)", { 12, 2, 0, 0, true });
    AddCommand(scsi_command::report_luns, 12, "REPORT LUNS", { 6, 4, 0, 0, false });
    AddCommand(scsi_command::blank, 12, "BLANK", { 0,0,0,0, false });
    AddCommand(scsi_command::play_audio_12, 12, "PLAY AUDIO(12)", { 6, 4, 2, 4, false });
    AddCommand(scsi_command::read_dvd_structure, 12, "READ DVD STRUCTURE", { 8, 2, 0, 0, false });
    AddCommand(scsi_command::read_defect_data_12, 12, "READ DEFECT DATA", { 6, 4, 0, 0, false });
    AddCommand(scsi_command::read_cd_msf, 12, "READ CD MSF", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::set_cd_speed, 12, "SET CD SPEED", { 0, 0, 0, 0, false });
    AddCommand(scsi_command::play_cd, 12, "PLAY CD", { 6, 4, 2, 4, false });
    AddCommand(scsi_command::read_cd, 12, "READ CD", { 6, 3, 2, 4, false });
    AddCommand(scsi_command::execute_operation, 10, "EXECUTE OPERATION (SCSI2Pi-specific)", { 7, 2, 0, 0, true });
    AddCommand(scsi_command::receive_operation_results, 10, "RECEIVE OPERATION RESULTS (SCSI2Pi-specific)",
        { 7, 2, 0, 0, false });
}

void CommandMetaData::AddCommand(scsi_command cmd, int byte_count, string_view name, const CdbMetaData &meta_data)
{
    assert(meta_data.allocation_length_offset <= 12);
    assert(meta_data.allocation_length_size <= 4);

    command_byte_counts[static_cast<int>(cmd)] = byte_count;
    command_names[static_cast<int>(cmd)] = name;
    cdb_meta_data[static_cast<int>(cmd)] = meta_data;
}

CommandMetaData::CdbMetaData CommandMetaData::GetCdbMetaData(scsi_command cmd) const
{
    return cdb_meta_data[static_cast<int>(cmd)];
}

string CommandMetaData::LogCdb(span<const uint8_t> cdb) const
{
    const string &command_name = GetCommandName(static_cast<scsi_command>(cdb[0]));
    string msg = fmt::format("Executing {}, CDB ", command_name, cdb[0]);

    for (size_t i = 0; i < cdb.size(); i++) {
        if (i) {
            msg += ":";
        }
        msg += fmt::format("{:02x}", cdb[i]);
    }

    return msg;
}
