//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/daynaport.h"
#include "shared/s2p_exceptions.h"

TEST(DaynaportTest, Device_Defaults)
{
    DaynaPort daynaport(0);

    EXPECT_EQ(SCDP, daynaport.GetType());
    EXPECT_FALSE(daynaport.SupportsFile());
    EXPECT_TRUE(daynaport.SupportsParams());
    EXPECT_FALSE(daynaport.IsProtectable());
    EXPECT_FALSE(daynaport.IsProtected());
    EXPECT_FALSE(daynaport.IsReadOnly());
    EXPECT_FALSE(daynaport.IsRemovable());
    EXPECT_FALSE(daynaport.IsRemoved());
    EXPECT_FALSE(daynaport.IsLockable());
    EXPECT_FALSE(daynaport.IsLocked());
    EXPECT_FALSE(daynaport.IsStoppable());
    EXPECT_FALSE(daynaport.IsStopped());

    EXPECT_EQ("Dayna", daynaport.GetVendor());
    EXPECT_EQ("SCSI/Link", daynaport.GetProduct());
    EXPECT_EQ("1.4a", daynaport.GetRevision());
}

TEST(DaynaportTest, GetDefaultParams)
{
    DaynaPort daynaport(0);

    const auto params = daynaport.GetDefaultParams();
    EXPECT_EQ(3U, params.size());
    EXPECT_TRUE(params.contains("interface"));
    EXPECT_TRUE(params.contains("inet"));
    EXPECT_TRUE(params.contains("bridge"));
}

TEST(DaynaportTest, Inquiry)
{
    TestShared::Inquiry(SCDP, device_type::processor, scsi_level::scsi_2, "Dayna   SCSI/Link       1.4a", 0x1f, false);
}

TEST(DaynaportTest, TestUnitReady)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(DaynaportTest, Read)
{
    DaynaPort daynaport(0);
    vector<int> cdb(6);

    // ALLOCATION LENGTH
    cdb[4] = 1;
    vector<uint8_t> buf;
    EXPECT_EQ(0, daynaport.Read(cdb, buf, 0)) << "Trying to read the root sector must fail";
}

TEST(DaynaportTest, WriteData)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    vector<int> cdb(6);

    // Unknown data format
    controller->SetCdbByte(5, 0xff);
    vector<uint8_t> buf(0);
    EXPECT_NO_THROW(dynamic_pointer_cast<DaynaPort>(daynaport)->WriteData(buf, scsi_command::cmd_write6));
}

TEST(DaynaportTest, Read6)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    controller->SetCdbByte(5, 0xff);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid data format");
}

TEST(DaynaportTest, Write6)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    controller->SetCdbByte(5, 0x00);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid transfer length");

    controller->SetCdbByte(3, -1);
    controller->SetCdbByte(4, -8);
    controller->SetCdbByte(5, 0x08);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid transfer length");

    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 0);
    controller->SetCdbByte(5, 0xff);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_write6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid transfer length");
}

TEST(DaynaportTest, TestRetrieveStats)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_retrieve_stats));
}

TEST(DaynaportTest, SetInterfaceMode)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_iface_mode, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Unknown interface command");

    // Not implemented, do nothing
    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SETMODE);
    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_set_iface_mode));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SETMAC);
    EXPECT_CALL(*controller, DataOut());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_set_iface_mode));

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_STATS);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_iface_mode, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Not implemented");

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_ENABLE);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_iface_mode, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Not implemented");

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SET);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_iface_mode, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Not implemented");
}

TEST(DaynaportTest, SetMcastAddr)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_mcast_addr, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Length of 0 is not supported");

    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, DataOut());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_set_mcast_addr));
}

TEST(DaynaportTest, EnableInterface)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // Enable
    controller->SetCdbByte(5, 0x80);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_enable_interface, sense_key::aborted_command,
        asc::daynaport_enable_interface);

    // Disable
    controller->SetCdbByte(5, 0x00);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_enable_interface, sense_key::aborted_command,
        asc::daynaport_disable_interface);
}

TEST(DaynaportTest, GetDelayAfterBytes)
{
    DaynaPort daynaport(0);

    EXPECT_EQ(6, daynaport.GetDelayAfterBytes());
}

TEST(DaynaportTest, GetStatistics)
{
    DaynaPort daynaport(0);

    const auto &statistics = daynaport.GetStatistics();
    EXPECT_EQ(2U, statistics.size());
    EXPECT_EQ("byte_read_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("byte_write_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
}
