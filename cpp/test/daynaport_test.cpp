//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "devices/daynaport.h"

TEST(ScsiDaynaportTest, Device_Defaults)
{
    DeviceFactory device_factory;

    auto device = device_factory.CreateDevice(UNDEFINED, 0, "daynaport");
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
    EXPECT_EQ(2, params.size());
}

TEST(ScsiDaynaportTest, Inquiry)
{
    TestShared::Inquiry(SCDP, device_type::processor, scsi_level::scsi_2, "Dayna   SCSI/Link       1.4a", 0x20, false);
}

TEST(ScsiDaynaportTest, TestUnitReady)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    EXPECT_CALL(*controller, Status());
    daynaport->Dispatch(scsi_command::cmd_test_unit_ready);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiDaynaportTest, Read)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // ALLOCATION LENGTH
    controller->SetCmdByte(4, 1);
    vector<uint8_t> buf(0);
    EXPECT_EQ(0, dynamic_pointer_cast<DaynaPort>(daynaport)->Read(controller->GetCmd(), buf, 0))
    << "Trying to read the root sector must fail";
}

TEST(ScsiDaynaportTest, Write)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // Unknown data format
    controller->SetCmdByte(5, 0xff);
    vector<uint8_t> buf(0);
    EXPECT_TRUE(dynamic_pointer_cast<DaynaPort>(daynaport)->Write(controller->GetCmd(), buf));
}

TEST(ScsiDaynaportTest, Read6)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    // Required by the bullseye clang++ compiler
    auto d = daynaport;

    controller->SetCmdByte(5, 0xff);
    EXPECT_THAT([&] {d->Dispatch(scsi_command::cmd_read6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Invalid data format";
}

TEST(ScsiDaynaportTest, Write6)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    // Required by the bullseye clang++ compiler
    auto d = daynaport;

    controller->SetCmdByte(5, 0x00);
    EXPECT_THAT([&] {d->Dispatch(scsi_command::cmd_write6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Invalid transfer length";

    controller->SetCmdByte(3, -1);
    controller->SetCmdByte(4, -8);
    controller->SetCmdByte(5, 0x08);
    EXPECT_THAT([&] {d->Dispatch(scsi_command::cmd_write6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Invalid transfer length";

    controller->SetCmdByte(3, 0);
    controller->SetCmdByte(4, 0);
    controller->SetCmdByte(5, 0xff);
    EXPECT_THAT([&] {d->Dispatch(scsi_command::cmd_write6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Invalid transfer length";
}

TEST(ScsiDaynaportTest, TestRetrieveStats)
{
    auto [controller, daynaport] = CreateDevice(SCDP);

    // ALLOCATION LENGTH
    controller->SetCmdByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    daynaport->Dispatch(scsi_command::cmd_retrieve_stats);
}

TEST(ScsiDaynaportTest, SetInterfaceMode)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    // Required by the bullseye clang++ compiler
    auto d = daynaport;

    // Unknown interface command
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_set_iface_mode)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

    // Not implemented, do nothing
    controller->SetCmdByte(5, DaynaPort::CMD_SCSILINK_SETMODE);
    EXPECT_CALL(*controller, Status());
    daynaport->Dispatch(scsi_command::cmd_set_iface_mode);
    EXPECT_EQ(status::good, controller->GetStatus());

    controller->SetCmdByte(5, DaynaPort::CMD_SCSILINK_SETMAC);
    EXPECT_CALL(*controller, DataOut());
    daynaport->Dispatch(scsi_command::cmd_set_iface_mode);

    // Not implemented
    controller->SetCmdByte(5, DaynaPort::CMD_SCSILINK_STATS);
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_set_iface_mode)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

    // Not implemented
    controller->SetCmdByte(5, DaynaPort::CMD_SCSILINK_ENABLE);
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_set_iface_mode)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

    // Not implemented
    controller->SetCmdByte(5, DaynaPort::CMD_SCSILINK_SET);
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_set_iface_mode)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));
}

TEST(ScsiDaynaportTest, SetMcastAddr)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    // Required by the bullseye clang++ compiler
    auto d = daynaport;

    EXPECT_THAT([&] {d->Dispatch(scsi_command::cmd_set_mcast_addr);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Length of 0 is not supported";

    controller->SetCmdByte(4, 1);
    EXPECT_CALL(*controller, DataOut());
    daynaport->Dispatch(scsi_command::cmd_set_mcast_addr);
}

TEST(ScsiDaynaportTest, EnableInterface)
{
    auto [controller, daynaport] = CreateDevice(SCDP);
    // Required by the bullseye clang++ compiler
    auto d = daynaport;

    // Enable
    controller->SetCmdByte(5, 0x80);
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_enable_interface)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
            Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));

    // Disable
    controller->SetCmdByte(5, 0x00);
    EXPECT_THAT([&]
        {
            d->Dispatch(scsi_command::cmd_enable_interface)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
            Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));
}

TEST(ScsiDaynaportTest, GetSendDelay)
{
    DaynaPort daynaport(0);
    daynaport.Init( { });

    EXPECT_EQ(6, daynaport.GetSendDelay());
}

TEST(ScsiDaynaportTest, GetStatistics)
{
    DaynaPort daynaport(0);

    EXPECT_EQ(2, daynaport.GetStatistics().size());
}
