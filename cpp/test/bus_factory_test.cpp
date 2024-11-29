//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "buses/bus_factory.h"

TEST(BusFactoryTest, CreateBus)
{
    auto bus = BusFactory::Instance().CreateBus(true, true);
    EXPECT_NE(nullptr, bus);
    // Avoid a delay by signalling the initiator that the target is ready
    bus->CleanUp();
    EXPECT_NE(nullptr, BusFactory::Instance().CreateBus(false, true));
}

TEST(BusFactoryTest, GetCommandBytesCount)
{
    const BusFactory &bus_factory = BusFactory::Instance();

    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::test_unit_ready));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::rezero));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::rewind));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::request_sense));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::format_unit));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::format_medium));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::read_block_limits));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::reassign_blocks));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::read_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::get_message_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::retrieve_stats));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::write_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::send_message_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::print));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::seek_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::set_iface_mode));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::set_mcast_addr));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::enable_interface));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::synchronize_buffer));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::write_filemarks_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::space_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::inquiry));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::mode_select_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::reserve_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::release_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::erase_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::mode_sense_6));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::start_stop));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::stop_print));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::send_diagnostic));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(scsi_command::prevent_allow_medium_removal));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_format_capacities));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_capacity_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::write_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::seek_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::locate_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::verify_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_position));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::synchronize_cache_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_defect_data_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_long_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::write_long_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::read_toc));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::mode_select_10));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::mode_sense_10));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::read_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::write_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::verify_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::synchronize_cache_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::locate_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::read_capacity_16_read_long_16));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(scsi_command::write_long_16));
    EXPECT_EQ(12, bus_factory.GetCommandBytesCount(scsi_command::report_luns));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::execute_operation));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(scsi_command::receive_operation_results));
    EXPECT_EQ(0, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1f)));

    int command_count = 0;
    for (int i = 0; i < 256; i++) {
        if (bus_factory.GetCommandBytesCount(static_cast<scsi_command>(i))) {
            ++command_count;
        }
    }
    EXPECT_EQ(48, command_count);
}

TEST(BusFactoryTest, GetCommandName)
{
    const BusFactory &bus_factory = BusFactory::Instance();

    EXPECT_EQ("TEST UNIT READY", bus_factory.GetCommandName(scsi_command::test_unit_ready));
    EXPECT_EQ("REZERO/REWIND", bus_factory.GetCommandName(scsi_command::rezero));
    EXPECT_EQ("READ BLOCK LIMITS", bus_factory.GetCommandName(scsi_command::read_block_limits));
    EXPECT_EQ("REQUEST SENSE", bus_factory.GetCommandName(scsi_command::request_sense));
    EXPECT_EQ("FORMAT UNIT/FORMAT MEDIUM", bus_factory.GetCommandName(scsi_command::format_unit));
    EXPECT_EQ("FORMAT UNIT/FORMAT MEDIUM", bus_factory.GetCommandName(scsi_command::format_medium));
    EXPECT_EQ("REASSIGN BLOCKS", bus_factory.GetCommandName(scsi_command::reassign_blocks));
    EXPECT_EQ("READ(6)/GET MESSAGE(6)", bus_factory.GetCommandName(scsi_command::read_6));
    EXPECT_EQ("READ(6)/GET MESSAGE(6)", bus_factory.GetCommandName(scsi_command::get_message_6));
    EXPECT_EQ("RETRIEVE STATS", bus_factory.GetCommandName(scsi_command::retrieve_stats));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::write_6));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::send_message_6));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::print));
    EXPECT_EQ("SEEK(6)", bus_factory.GetCommandName(scsi_command::seek_6));
    EXPECT_EQ("SET INTERFACE MODE", bus_factory.GetCommandName(scsi_command::set_iface_mode));
    EXPECT_EQ("SET MULTICAST ADDRESS", bus_factory.GetCommandName(scsi_command::set_mcast_addr));
    EXPECT_EQ("ENABLE INTERFACE", bus_factory.GetCommandName(scsi_command::enable_interface));
    EXPECT_EQ("SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)",
        bus_factory.GetCommandName(scsi_command::synchronize_buffer));
    EXPECT_EQ("SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)", bus_factory.GetCommandName(scsi_command::write_filemarks_6));
    EXPECT_EQ("SPACE(6)", bus_factory.GetCommandName(scsi_command::space_6));
    EXPECT_EQ("INQUIRY", bus_factory.GetCommandName(scsi_command::inquiry));
    EXPECT_EQ("MODE SELECT(6)", bus_factory.GetCommandName(scsi_command::mode_select_6));
    EXPECT_EQ("RESERVE(6)", bus_factory.GetCommandName(scsi_command::reserve_6));
    EXPECT_EQ("RELEASE(6)", bus_factory.GetCommandName(scsi_command::release_6));
    EXPECT_EQ("ERASE(6)", bus_factory.GetCommandName(scsi_command::erase_6));
    EXPECT_EQ("MODE SENSE(6)", bus_factory.GetCommandName(scsi_command::mode_sense_6));
    EXPECT_EQ("START STOP UNIT/STOP PRINT", bus_factory.GetCommandName(scsi_command::start_stop));
    EXPECT_EQ("START STOP UNIT/STOP PRINT", bus_factory.GetCommandName(scsi_command::stop_print));
    EXPECT_EQ("SEND DIAGNOSTIC", bus_factory.GetCommandName(scsi_command::send_diagnostic));
    EXPECT_EQ("PREVENT ALLOW MEDIUM REMOVAL",
        bus_factory.GetCommandName(scsi_command::prevent_allow_medium_removal));
    EXPECT_EQ("READ FORMAT CAPACITIES", bus_factory.GetCommandName(scsi_command::read_format_capacities));
    EXPECT_EQ("READ CAPACITY(10)", bus_factory.GetCommandName(scsi_command::read_capacity_10));
    EXPECT_EQ("READ(10)", bus_factory.GetCommandName(scsi_command::read_10));
    EXPECT_EQ("WRITE(10)", bus_factory.GetCommandName(scsi_command::write_10));
    EXPECT_EQ("SEEK(10)/LOCATE(10)", bus_factory.GetCommandName(scsi_command::seek_10));
    EXPECT_EQ("SEEK(10)/LOCATE(10)", bus_factory.GetCommandName(scsi_command::locate_10));
    EXPECT_EQ("READ POSITION", bus_factory.GetCommandName(scsi_command::read_position));
    EXPECT_EQ("VERIFY(10)", bus_factory.GetCommandName(scsi_command::verify_10));
    EXPECT_EQ("SYNCHRONIZE CACHE(10)", bus_factory.GetCommandName(scsi_command::synchronize_cache_10));
    EXPECT_EQ("READ DEFECT DATA(10)", bus_factory.GetCommandName(scsi_command::read_defect_data_10));
    EXPECT_EQ("READ LONG(10)", bus_factory.GetCommandName(scsi_command::read_long_10));
    EXPECT_EQ("WRITE LONG(10)", bus_factory.GetCommandName(scsi_command::write_long_10));
    EXPECT_EQ("READ TOC", bus_factory.GetCommandName(scsi_command::read_toc));
    EXPECT_EQ("MODE SELECT(10)", bus_factory.GetCommandName(scsi_command::mode_select_10));
    EXPECT_EQ("MODE SENSE(10)", bus_factory.GetCommandName(scsi_command::mode_sense_10));
    EXPECT_EQ("READ(16)", bus_factory.GetCommandName(scsi_command::read_16));
    EXPECT_EQ("WRITE(16)", bus_factory.GetCommandName(scsi_command::write_16));
    EXPECT_EQ("VERIFY(16)", bus_factory.GetCommandName(scsi_command::verify_16));
    EXPECT_EQ("SYNCHRONIZE CACHE(16)", bus_factory.GetCommandName(scsi_command::synchronize_cache_16));
    EXPECT_EQ("READ CAPACITY(16)/READ LONG(16)",
        bus_factory.GetCommandName(scsi_command::read_capacity_16_read_long_16));
    EXPECT_EQ("LOCATE(16)", bus_factory.GetCommandName(scsi_command::locate_16));
    EXPECT_EQ("WRITE LONG(16)", bus_factory.GetCommandName(scsi_command::write_long_16));
    EXPECT_EQ("REPORT LUNS", bus_factory.GetCommandName(scsi_command::report_luns));
    EXPECT_EQ("EXECUTE OPERATION (SCSI2Pi-specific)", bus_factory.GetCommandName(scsi_command::execute_operation));
    EXPECT_EQ("RECEIVE OPERATION RESULTS (SCSI2Pi-specific)",
        bus_factory.GetCommandName(scsi_command::receive_operation_results));
}
