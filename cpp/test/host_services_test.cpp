//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_exceptions.h"

static void SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(1U, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(10U, pages[32].size());
}

TEST(HostServicesTest, DeviceDefaults)
{
    HostServices services(0);

    EXPECT_EQ(SCHS, services.GetType());
    EXPECT_FALSE(services.SupportsFile());
    EXPECT_FALSE(services.SupportsParams());
    EXPECT_FALSE(services.IsProtectable());
    EXPECT_FALSE(services.IsProtected());
    EXPECT_FALSE(services.IsReadOnly());
    EXPECT_FALSE(services.IsRemovable());
    EXPECT_FALSE(services.IsRemoved());
    EXPECT_FALSE(services.IsLockable());
    EXPECT_FALSE(services.IsLocked());
    EXPECT_FALSE(services.IsStoppable());
    EXPECT_FALSE(services.IsStopped());

    EXPECT_EQ("SCSI2Pi", services.GetVendor());
    EXPECT_EQ("Host Services", services.GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), services.GetRevision());
}

TEST(HostServicesTest, TestUnitReady)
{
    auto [controller, services] = CreateDevice(SCHS);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(HostServicesTest, Inquiry)
{
    TestShared::Inquiry(SCHS, device_type::processor, scsi_level::spc_3, "SCSI2Pi Host Services   ", 0x1f, false);
}

TEST(HostServicesTest, StartStopUnit)
{
    auto [controller, services] = CreateDevice(SCHS);

    // STOP
    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // LOAD
    controller->SetCdbByte(4, 0x02);
    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // UNLOAD
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // START
    controller->SetCdbByte(4, 0x01);
    TestShared::Dispatch(*services, scsi_command::cmd_start_stop, sense_key::illegal_request,
        asc::invalid_field_in_cdb);
}

TEST(HostServicesTest, ExecuteOperation)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    TestShared::Dispatch(*services, scsi_command::cmd_execute_operation, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Illegal format");

    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*services, scsi_command::cmd_execute_operation, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Illegal format");

    controller->SetCdbByte(1, 0b001);
    TestShared::Dispatch(*services, scsi_command::cmd_execute_operation, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Illegal length");
}

TEST(HostServicesTest, ReceiveOperationResults)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    TestShared::Dispatch(*services, scsi_command::cmd_receive_operation_results, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Illegal format");

    controller->SetCdbByte(1, 0b111);
    TestShared::Dispatch(*services, scsi_command::cmd_receive_operation_results, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Illegal format");

    controller->SetCdbByte(1, 0b010);
    TestShared::Dispatch(*services, scsi_command::cmd_receive_operation_results, sense_key::aborted_command,
        asc::host_services_receive_operation_results, "No matching initiator ID");
}

TEST(HostServicesTest, ModeSense6)
{
    auto [controller, services] = CreateDevice(SCHS);

    TestShared::Dispatch(*services, scsi_command::cmd_mode_sense6, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    TestShared::Dispatch(*services, scsi_command::cmd_mode_sense6, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_mode_sense6));
    vector<uint8_t> &buffer = controller->GetBuffer();
    // Major version 1
    EXPECT_EQ(0x01, buffer[6]);
    // Minor version 0
    EXPECT_EQ(0x00, buffer[7]);
    // Year
    EXPECT_NE(0x00, buffer[8]);
    // Day
    EXPECT_NE(0x00, buffer[10]);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 2);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_mode_sense6));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x01, buffer[0]);
}

TEST(HostServicesTest, ModeSense10)
{
    auto [controller, services] = CreateDevice(SCHS);

    TestShared::Dispatch(*services, scsi_command::cmd_mode_sense10, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    TestShared::Dispatch(*services, scsi_command::cmd_mode_sense10, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_mode_sense10));
    vector<uint8_t> &buffer = controller->GetBuffer();
    // Major version 1
    EXPECT_EQ(0x01, buffer[10]);
    // Minor version 0
    EXPECT_EQ(0x00, buffer[11]);
    // Year
    EXPECT_NE(0x00, buffer[12]);
    // Day
    EXPECT_NE(0x00, buffer[14]);

    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 4);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(services->Dispatch(scsi_command::cmd_mode_sense10));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x02, buffer[1]);
}

TEST(HostServicesTest, SetUpModePages)
{
    MockHostServices services(0);
    map<int, vector<byte>> pages;

    // Non changeable
    services.SetUpModePages(pages, 0x3f, false);
    SetUpModePages(pages);

    // Changeable
    pages.clear();
    services.SetUpModePages(pages, 0x3f, true);
    SetUpModePages(pages);
}

TEST(HostServicesTest, WriteData)
{
    auto [controller, services] = CreateDevice(SCHS);

    EXPECT_EQ(0, services->WriteData( { }, scsi_command::cmd_execute_operation));
}
