//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "base/device_factory.h"
#include "devices/daynaport.h"

TEST(ScsiDaynaportTest, Device_Defaults)
{
    auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "daynaport");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCDP, device->GetType());
    EXPECT_FALSE(device->SupportsFile());
    EXPECT_TRUE(device->SupportsParams());
    EXPECT_FALSE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_FALSE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_FALSE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("Dayna", device->GetVendor());
    EXPECT_EQ("SCSI/Link", device->GetProduct());
    EXPECT_EQ("1.4a", device->GetRevision());
}

TEST(ScsiDaynaportTest, GetDefaultParams)
{
    const auto [controller, daynaport] = CreateDevice(SCDP);
    const auto params = daynaport->GetDefaultParams();
    EXPECT_EQ(2U, params.size());
}

TEST(ScsiDaynaportTest, Inquiry)
{
    TestShared::Inquiry(SCDP, device_type::processor, scsi_level::scsi_2, "Dayna   SCSI/Link       1.4a", 0x1f, false);
}

TEST(ScsiDaynaportTest, TestUnitReady)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiDaynaportTest, Read)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 1);
    vector<uint8_t> buf(0);
    EXPECT_EQ(0, dynamic_pointer_cast<DaynaPort>(daynaport)->Read(controller->GetCdb(), buf, 0))
        << "Trying to read the root sector must fail";
}

TEST(ScsiDaynaportTest, Write)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // Unknown data format
    controller->SetCdbByte(5, 0xff);
    vector<uint8_t> buf(0);
    EXPECT_NO_THROW(dynamic_pointer_cast<DaynaPort>(daynaport)->WriteData(buf, false));
}

TEST(ScsiDaynaportTest, Read6)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    controller->SetCdbByte(5, 0xff);
    TestShared::Dispatch(*daynaport, scsi_command::cmd_read6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Invalid data format");
}

TEST(ScsiDaynaportTest, Write6)
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

TEST(ScsiDaynaportTest, TestRetrieveStats)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_retrieve_stats));
}

TEST(ScsiDaynaportTest, SetInterfaceMode)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_iface_mode, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Unknown interface command");

    // Not implemented, do nothing
    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SETMODE);
    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_set_iface_mode));
    EXPECT_EQ(status::good, controller->GetStatus());

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

TEST(ScsiDaynaportTest, SetMcastAddr)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    TestShared::Dispatch(*daynaport, scsi_command::cmd_set_mcast_addr, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Length of 0 is not supported");

    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, DataOut());
    EXPECT_NO_THROW(daynaport->Dispatch(scsi_command::cmd_set_mcast_addr));
}

TEST(ScsiDaynaportTest, EnableInterface)
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

TEST(ScsiDaynaportTest, GetDelayAfterBytes)
{
    DaynaPort daynaport(0);
    daynaport.Init( { });

    EXPECT_EQ(6, daynaport.GetDelayAfterBytes());
}

TEST(ScsiDaynaportTest, GetStatistics)
{
    DaynaPort daynaport(0);

    EXPECT_EQ(2U, daynaport.GetStatistics().size());
}
