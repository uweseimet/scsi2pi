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
    auto target = BusFactory::Instance().CreateBus(true, true);
    EXPECT_NE(nullptr, target);
    // Avoid a delay by signalling the initiator that the target is ready
    target->CleanUp();
    EXPECT_NE(nullptr, BusFactory::Instance().CreateBus(false, true));
}

TEST(BusFactoryTest, GetCommandBytesCount)
{
    const BusFactory &bus_factory = BusFactory::Instance();

    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x00)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x01)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x03)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x04)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x07)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x08)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x09)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x0a)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x0b)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x0c)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x0d)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x0e)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x10)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x12)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x15)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x16)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x17)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1a)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1b)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1d)));
    EXPECT_EQ(6, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1e)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x25)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x28)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x2a)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x2b)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x2f)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x34)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x35)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x37)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x3e)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x3f)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x43)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x55)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x5a)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x88)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x8a)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x8f)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x91)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x92)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x9e)));
    EXPECT_EQ(16, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x9f)));
    EXPECT_EQ(12, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0xa0)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0xc0)));
    EXPECT_EQ(10, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0xc1)));
    EXPECT_EQ(0, bus_factory.GetCommandBytesCount(static_cast<scsi_command>(0x1f)));

    int command_count = 0;
    for (int i = 0; i < 256; i++) {
        if (bus_factory.GetCommandBytesCount(static_cast<scsi_command>(i))) {
            ++command_count;
        }
    }
    EXPECT_EQ(47, command_count);
}

TEST(BusFactoryTest, GetCommandName)
{
    const BusFactory &bus_factory = BusFactory::Instance();

    EXPECT_EQ("TEST UNIT READY", bus_factory.GetCommandName(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ("REZERO/REWIND", bus_factory.GetCommandName(scsi_command::cmd_rezero));
    EXPECT_EQ("READ BLOCK LIMITS", bus_factory.GetCommandName(scsi_command::cmd_read_block_limits));
    EXPECT_EQ("REQUEST SENSE", bus_factory.GetCommandName(scsi_command::cmd_request_sense));
    EXPECT_EQ("FORMAT UNIT/FORMAT MEDIUM", bus_factory.GetCommandName(scsi_command::cmd_format_unit));
    EXPECT_EQ("FORMAT UNIT/FORMAT MEDIUM", bus_factory.GetCommandName(scsi_command::cmd_format_medium));
    EXPECT_EQ("REASSIGN BLOCKS", bus_factory.GetCommandName(scsi_command::cmd_reassign_blocks));
    EXPECT_EQ("READ(6)/GET MESSAGE(6)", bus_factory.GetCommandName(scsi_command::cmd_read6));
    EXPECT_EQ("READ(6)/GET MESSAGE(6)", bus_factory.GetCommandName(scsi_command::cmd_get_message6));
    EXPECT_EQ("RETRIEVE STATS", bus_factory.GetCommandName(scsi_command::cmd_retrieve_stats));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::cmd_write6));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::cmd_send_message6));
    EXPECT_EQ("WRITE(6)/SEND MESSAGE(6)/PRINT", bus_factory.GetCommandName(scsi_command::cmd_print));
    EXPECT_EQ("SEEK(6)", bus_factory.GetCommandName(scsi_command::cmd_seek6));
    EXPECT_EQ("SET INTERFACE MODE", bus_factory.GetCommandName(scsi_command::cmd_set_iface_mode));
    EXPECT_EQ("SET MULTICAST ADDRESS", bus_factory.GetCommandName(scsi_command::cmd_set_mcast_addr));
    EXPECT_EQ("ENABLE INTERFACE", bus_factory.GetCommandName(scsi_command::cmd_enable_interface));
    EXPECT_EQ("SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)",
        bus_factory.GetCommandName(scsi_command::cmd_synchronize_buffer));
    EXPECT_EQ("SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)", bus_factory.GetCommandName(scsi_command::cmd_write_filemarks6));
    EXPECT_EQ("SPACE(6)", bus_factory.GetCommandName(scsi_command::cmd_space6));
    EXPECT_EQ("INQUIRY", bus_factory.GetCommandName(scsi_command::cmd_inquiry));
    EXPECT_EQ("MODE SELECT(6)", bus_factory.GetCommandName(scsi_command::cmd_mode_select6));
    EXPECT_EQ("RESERVE(6)", bus_factory.GetCommandName(scsi_command::cmd_reserve6));
    EXPECT_EQ("RELEASE(6)", bus_factory.GetCommandName(scsi_command::cmd_release6));
    EXPECT_EQ("ERASE(6)", bus_factory.GetCommandName(scsi_command::cmd_erase6));
    EXPECT_EQ("MODE SENSE(6)", bus_factory.GetCommandName(scsi_command::cmd_mode_sense6));
    EXPECT_EQ("START STOP UNIT/STOP PRINT", bus_factory.GetCommandName(scsi_command::cmd_start_stop));
    EXPECT_EQ("START STOP UNIT/STOP PRINT", bus_factory.GetCommandName(scsi_command::cmd_stop_print));
    EXPECT_EQ("SEND DIAGNOSTIC", bus_factory.GetCommandName(scsi_command::cmd_send_diagnostic));
    EXPECT_EQ("PREVENT ALLOW MEDIUM REMOVAL",
        bus_factory.GetCommandName(scsi_command::cmd_prevent_allow_medium_removal));
    EXPECT_EQ("READ CAPACITY(10)", bus_factory.GetCommandName(scsi_command::cmd_read_capacity10));
    EXPECT_EQ("READ(10)", bus_factory.GetCommandName(scsi_command::cmd_read10));
    EXPECT_EQ("WRITE(10)", bus_factory.GetCommandName(scsi_command::cmd_write10));
    EXPECT_EQ("SEEK(10)/LOCATE(10)", bus_factory.GetCommandName(scsi_command::cmd_seek10));
    EXPECT_EQ("SEEK(10)/LOCATE(10)", bus_factory.GetCommandName(scsi_command::cmd_locate10));
    EXPECT_EQ("READ POSITION", bus_factory.GetCommandName(scsi_command::cmd_read_position));
    EXPECT_EQ("VERIFY(10)", bus_factory.GetCommandName(scsi_command::cmd_verify10));
    EXPECT_EQ("SYNCHRONIZE CACHE(10)", bus_factory.GetCommandName(scsi_command::cmd_synchronize_cache10));
    EXPECT_EQ("READ DEFECT DATA(10)", bus_factory.GetCommandName(scsi_command::cmd_read_defect_data10));
    EXPECT_EQ("READ LONG(10)", bus_factory.GetCommandName(scsi_command::cmd_read_long10));
    EXPECT_EQ("WRITE LONG(10)", bus_factory.GetCommandName(scsi_command::cmd_write_long10));
    EXPECT_EQ("READ TOC", bus_factory.GetCommandName(scsi_command::cmd_read_toc));
    EXPECT_EQ("MODE SELECT(10)", bus_factory.GetCommandName(scsi_command::cmd_mode_select10));
    EXPECT_EQ("MODE SENSE(10)", bus_factory.GetCommandName(scsi_command::cmd_mode_sense10));
    EXPECT_EQ("READ(16)", bus_factory.GetCommandName(scsi_command::cmd_read16));
    EXPECT_EQ("WRITE(16)", bus_factory.GetCommandName(scsi_command::cmd_write16));
    EXPECT_EQ("VERIFY(16)", bus_factory.GetCommandName(scsi_command::cmd_verify16));
    EXPECT_EQ("SYNCHRONIZE CACHE(16)", bus_factory.GetCommandName(scsi_command::cmd_synchronize_cache16));
    EXPECT_EQ("READ CAPACITY(16)/READ LONG(16)",
        bus_factory.GetCommandName(scsi_command::cmd_read_capacity16_read_long16));
    EXPECT_EQ("LOCATE(16)", bus_factory.GetCommandName(scsi_command::cmd_locate16));
    EXPECT_EQ("WRITE LONG(16)", bus_factory.GetCommandName(scsi_command::cmd_write_long16));
    EXPECT_EQ("REPORT LUNS", bus_factory.GetCommandName(scsi_command::cmd_report_luns));
    EXPECT_EQ("EXECUTE OPERATION (SCSI2Pi-specific)", bus_factory.GetCommandName(scsi_command::cmd_execute_operation));
    EXPECT_EQ("RECEIVE OPERATION RESULTS (SCSI2Pi-specific)",
        bus_factory.GetCommandName(scsi_command::cmd_receive_operation_results));
}
