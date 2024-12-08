//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/command_meta_data.h"

TEST(CommandMetaDataTest, GetCommandBytesCount)
{
    const CommandMetaData &meta_data = CommandMetaData::Instance();

    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::test_unit_ready));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::rezero));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::rewind));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::request_sense));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::format_unit));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::format_medium));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::read_block_limits));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::reassign_blocks));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::read_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::get_message_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::retrieve_stats));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::write_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::send_message_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::print));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::seek_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::set_iface_mode));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::set_mcast_addr));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::enable_interface));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::synchronize_buffer));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::write_filemarks_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::space_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::inquiry));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::mode_select_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::reserve_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::release_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::erase_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::mode_sense_6));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::start_stop));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::stop_print));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::send_diagnostic));
    EXPECT_EQ(6, meta_data.GetCommandBytesCount(scsi_command::prevent_allow_medium_removal));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_format_capacities));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_capacity_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::write_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::seek_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::locate_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::verify_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_position));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::synchronize_cache_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_defect_data_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_long_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::write_long_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::read_toc));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::mode_select_10));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::mode_sense_10));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::read_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::write_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::verify_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::synchronize_cache_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::locate_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::read_capacity_16_read_long_16));
    EXPECT_EQ(16, meta_data.GetCommandBytesCount(scsi_command::write_long_16));
    EXPECT_EQ(12, meta_data.GetCommandBytesCount(scsi_command::report_luns));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::execute_operation));
    EXPECT_EQ(10, meta_data.GetCommandBytesCount(scsi_command::receive_operation_results));
    EXPECT_EQ(0, meta_data.GetCommandBytesCount(static_cast<scsi_command>(0x1f)));

    int command_count = 0;
    for (int i = 0; i < 256; i++) {
        if (meta_data.GetCommandBytesCount(static_cast<scsi_command>(i))) {
            ++command_count;
        }
    }
    EXPECT_EQ(48, command_count);
}

TEST(CommandMetaDataTest, LogCdb)
{

    const array<const uint8_t, 6> &cdb = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    EXPECT_EQ("type is executing REZERO/REWIND, CDB 01:02:03:04:05:06",
        CommandMetaData::Instance().LogCdb(cdb, "type"));
}
