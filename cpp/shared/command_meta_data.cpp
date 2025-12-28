//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_meta_data.h"
#include <cassert>
#include <sstream>
#include <spdlog/spdlog.h>

using namespace spdlog;

CommandMetaData::CommandMetaData()
{
    for (int i = 0x00; i < 0x1f; ++i) {
        AddCommand(static_cast<ScsiCommand>(i), 6, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0x20; i < 0x7f; ++i) {
        AddCommand(static_cast<ScsiCommand>(i), 10, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0x80; i < 0xa0; ++i) {
        AddCommand(static_cast<ScsiCommand>(i), 16, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    for (int i = 0xa0; i < 0xc0; ++i) {
        AddCommand(static_cast<ScsiCommand>(i), 12, fmt::format("command ${:02x}", i), { 0, 0, 0, 0, false });
    }

    // This mapping contains all commands supported by s2p (see https://www.scsi2pi.net/en/scsi_commands.html)
    // and some others typically used with the SCSG device
    AddCommand(ScsiCommand::TEST_UNIT_READY, 6, "TEST UNIT READY", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::REZERO, 6, "REZERO/REWIND", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_BLOCK_LIMITS, 6, "READ BLOCK LIMITS", { -6, 0, 0, 0, false });
    AddCommand(ScsiCommand::REQUEST_SENSE, 6, "REQUEST SENSE", { 4, 1, 0, 0, false });
    AddCommand(ScsiCommand::FORMAT_UNIT, 6, "FORMAT UNIT/FORMAT MEDIUM", { 0, 0, 0, 0, true });
    AddCommand(ScsiCommand::REASSIGN_BLOCKS, 6, "REASSIGN BLOCKS", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_6, 6, "READ(6)/GET MESSAGE(6)", { 4, 1, 1, 3, false });
    AddCommand(ScsiCommand::RETRIEVE_STATS, 6, "RETRIEVE STATS", { 4, 1, 0, 0, false });
    AddCommand(ScsiCommand::WRITE_6, 6, "WRITE(6)/SEND MESSAGE(6)/PRINT", { 4, 1, 1, 3, true });
    AddCommand(ScsiCommand::SEEK_6, 6, "SEEK(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::SET_IFACE_MODE, 6, "SET INTERFACE MODE", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::SET_MCAST_ADDR, 6, "SET MULTICAST ADDRESS", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::ENABLE_INTERFACE, 6, "ENABLE INTERFACE", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_REVERSE, 6, "READ REVERSE(6)", { 4, 1, 1, 3, false });
    AddCommand(ScsiCommand::SYNCHRONIZE_BUFFER, 6, "SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::SPACE_6, 6, "SPACE(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::INQUIRY, 6, "INQUIRY", { 4, 1, 0, 0, false });
    AddCommand(ScsiCommand::VERIFY_6, 6, "VERIFY(6)", { 4, 1, 1, 3, true });
    AddCommand(ScsiCommand::MODE_SELECT_6, 6, "MODE SELECT(6)", { 4, 1, 0, 0, true });
    AddCommand(ScsiCommand::RESERVE_RESERVE_ELEMENT_6, 6, "RESERVE(6)(RESERVE ELEMENT(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::RELEASE_RELEASE_ELEMENT_6, 6, "RELEASE(6)/RELEASE ELEMENT(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::ERASE_6, 6, "ERASE(6)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::MODE_SENSE_6, 6, "MODE SENSE(6)", { 4, 1, 0, 0, false });
    AddCommand(ScsiCommand::START_STOP, 6, "START STOP UNIT/STOP PRINT", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::SEND_DIAGNOSTIC, 6, "SEND DIAGNOSTIC", { 3, 2, 0, 0, false });
    AddCommand(ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL, 6, "PREVENT ALLOW MEDIUM REMOVAL", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_FORMAT_CAPACITIES, 10, "READ FORMAT CAPACITIES", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_CAPACITY_10, 10, "READ CAPACITY(10)", { -8, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_10, 10, "READ(10)", { 7, 2, 2, 4, false });
    AddCommand(ScsiCommand::WRITE_10, 10, "WRITE(10)", { 7, 2, 2, 4, true });
    AddCommand(ScsiCommand::SEEK_10, 10, "SEEK(10)/LOCATE(10)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::ERASE_10, 10, "ERASE(10)", { 7, 2, 2, 4, false });
    AddCommand(ScsiCommand::WRITE_AND_VERIFY_10, 10, "WRITE AND VERIFY(10)", { 7, 2, 2, 4, true });
    AddCommand(ScsiCommand::VERIFY_10, 10, "VERIFY(10)", { 7, 2, 2, 4, true });
    AddCommand(ScsiCommand::READ_POSITION, 10, "READ POSITION", { -20, 0, 0, 0, false });
    AddCommand(ScsiCommand::SYNCHRONIZE_CACHE_10, 10, "SYNCHRONIZE CACHE(10)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_DEFECT_DATA_10, 10, "READ DEFECT DATA(10)", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::MEDIUM_SCAN, 10, "MEDIUM SCAN", { 8, 1, 2, 4, true });
    AddCommand(ScsiCommand::WRITE_BUFFER, 10, "READ BUFFER", { 6, 3, 0, 0, true });
    AddCommand(ScsiCommand::READ_BUFFER_10, 10, "READ BUFFER(10)", { 6, 3, 0, 0, false });
    AddCommand(ScsiCommand::READ_LONG_10, 10, "READ LONG(10)", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::WRITE_LONG_10, 10, "WRITE LONG(10)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::WRITE_SAME_10, 10, "WRITE SAME(10)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::READ_SUB_CHANNEL, 10, "READ SUB-CHANNEL", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_TOC, 10, "READ TOC", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_HEADER, 10, "READ HEADER", { 7, 2, 2, 4, false });
    AddCommand(ScsiCommand::PLAY_AUDIO_10, 10, "PLAY AUDIO(10)", { 7, 2, 2, 4, false });
    AddCommand(ScsiCommand::GET_CONFIGURATION, 10, "GET CONFIGURATION", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::PLAY_AUDIO_MSF, 10, "PLAY AUDIO MSF", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::PLAY_AUDIO_TRACK_INDEX, 10, "PLAY AUDIO TRACK/INDEX", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::GET_EVENT_STATUS_NOTIFICATION, 10, "GET EVENT/STATUS NOTIFICATION", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::PAUSE_RESUME, 10, "PAUSE/RESUME", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::LOG_SELECT, 10, "LOG SELECT", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::LOG_SENSE, 10, "LOG SENSE", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_DISC_INFORMATION, 10, "READ DISC INFORMATION", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_TRACK_INFORMATION, 10, "READ TRACk INFORMATION", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::RESERVE_RESERVE_ELEMENT_10, 10, "RESERVE(10)/RESERVE ELEMENT(10)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::MODE_SELECT_10, 10, "MODE SELECT(10)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::RELEASE_RELEASE_ELEMENT_10, 10, "RELEASE(10)/RELEASE ELEMENT(10)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::READ_MASTER_CUE, 10, "READ MASTER CUE", { 6, 3, 0, 0, false });
    AddCommand(ScsiCommand::MODE_SENSE_10, 10, "MODE SENSE(10)", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::CLOSE_TRACK_SESSION, 10, "CLOSE TRACK/SESSION", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_BUFFER_CAPACITY, 10, "READ BUFFER CAPACITY", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::PERSISTENT_RESERVE_IN, 10, "PERSISTENT RESERVE IN", { 7, 2, 0, 0, false });
    AddCommand(ScsiCommand::PERSISTENT_RESERVE_OUT, 10, "PERSISTENT RESERVE OUT", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::WRITE_FILEMARKS_16, 16, "WRITE FILEMARKS(16)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::REBUILD_READ_REVERSE_16, 16, "REBUILD(16)/READ REVERSE(16)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_16, 16, "READ(16)", { 10, 4, 2, 8, false });
    AddCommand(ScsiCommand::WRITE_16, 16, "WRITE(16)", { 10, 4, 2, 8, true });
    AddCommand(ScsiCommand::WRITE_AND_VERIFY_16, 16, "WRITE AND VERIFY(16)", { 10, 4, 2, 8, true });
    AddCommand(ScsiCommand::VERIFY_16, 16, "VERIFY(16)", { 10, 4, 2, 8, true });
    AddCommand(ScsiCommand::SYNCHRONIZE_CACHE_SPACE_16, 16, "SYNCHRONIZE CACHE(16)/SPACE(16)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::LOCATE_16, 16, "LOCATE(16)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::ERASE_WRITE_SAME_16, 16, "ERASE(16)/WRITE SAME(16)", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::READ_BUFFER_16, 16, "READ BUFFER(16)", { 10, 4, 0, 0, false });
    AddCommand(ScsiCommand::READ_CAPACITY_READ_LONG_16, 16, "READ CAPACITY(16)/READ LONG(16)",
        { 12, 2, 0, 0, false });
    AddCommand(ScsiCommand::WRITE_LONG_16, 16, "WRITE LONG(16)", { 12, 2, 0, 0, true });
    AddCommand(ScsiCommand::REPORT_LUNS, 12, "REPORT LUNS", { 6, 4, 0, 0, false });
    AddCommand(ScsiCommand::BLANK, 12, "BLANK", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::PLAY_AUDIO_12, 12, "PLAY AUDIO(12)", { 6, 4, 2, 4, false });
    AddCommand(ScsiCommand::READ_12, 12, "READ(12)", { 6, 4, 2, 4, false });
    AddCommand(ScsiCommand::WRITE_12, 12, "WRITE(12)", { 6, 4, 2, 4, true });
    AddCommand(ScsiCommand::ERASE_12, 12, "ERASE(12)", { 6, 4, 2, 4, false });
    AddCommand(ScsiCommand::READ_DVD_STRUCTURE, 12, "READ DVD STRUCTURE", { 8, 2, 0, 0, false });
    AddCommand(ScsiCommand::WRITE_AND_VERIFY_12, 12, "WRITE AND VERIFY(12)", { 6, 4, 2, 4, true });
    AddCommand(ScsiCommand::VERIFY_12, 12, "VERIFY(12)", { 6, 4, 2, 4, true });
    AddCommand(ScsiCommand::SEND_VOLUME_TAG, 12, "SEND VOLUME TAG", { 8, 2, 0, 0, false });
    AddCommand(ScsiCommand::READ_DEFECT_DATA_12, 12, "READ DEFECT DATA", { 6, 4, 0, 0, false });
    AddCommand(ScsiCommand::READ_CD_MSF, 12, "READ CD MSF", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::SET_CD_SPEED, 12, "SET CD SPEED", { 0, 0, 0, 0, false });
    AddCommand(ScsiCommand::PLAY_CD, 12, "PLAY CD", { 6, 4, 2, 4, false });
    AddCommand(ScsiCommand::READ_CD, 12, "READ CD", { 6, 3, 2, 4, false });
    AddCommand(ScsiCommand::EXECUTE_OPERATION, 10, "EXECUTE OPERATION (SCSI2Pi-specific)", { 7, 2, 0, 0, true });
    AddCommand(ScsiCommand::RECEIVE_OPERATION_RESULTS, 10, "RECEIVE OPERATION RESULTS (SCSI2Pi-specific)",
        { 7, 2, 0, 0, false });
}

void CommandMetaData::AddCommand(ScsiCommand cmd, int byte_count, string_view name, const CdbMetaData &meta_data)
{
    assert(meta_data.allocation_length_offset <= 12);
    assert(meta_data.allocation_length_size <= 4);

    command_byte_counts[static_cast<int>(cmd)] = byte_count;
    command_names[static_cast<int>(cmd)] = name;
    cdb_meta_data[static_cast<int>(cmd)] = meta_data;
}

CommandMetaData::CdbMetaData CommandMetaData::GetCdbMetaData(ScsiCommand cmd) const
{
    return cdb_meta_data[static_cast<int>(cmd)];
}

string CommandMetaData::LogCdb(span<const uint8_t> cdb, string_view type) const
{
    ostringstream msg;

    msg << type << " is executing " << GetCommandName(static_cast<ScsiCommand>(cdb[0])) << ", CDB ";

    for (size_t i = 0; i < cdb.size(); ++i) {
        if (i) {
            msg << ":";
        }
        msg << fmt::format("{:02x}", cdb[i]);
    }

    return msg.str();
}
