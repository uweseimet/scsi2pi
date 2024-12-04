//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/s2p_exceptions.h"

static void ValidateModePages(map<int, vector<byte>> &pages)
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

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::test_unit_ready));
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
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // LOAD
    controller->SetCdbByte(4, 0x02);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // UNLOAD
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::start_stop));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    // START
    controller->SetCdbByte(4, 0x01);
    Dispatch(*services, scsi_command::start_stop, sense_key::illegal_request, asc::invalid_field_in_cdb);
}

TEST(HostServicesTest, ExecuteOperation)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    Dispatch(*services, scsi_command::execute_operation, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Illegal format");

    controller->SetCdbByte(1, 0b111);
    Dispatch(*services, scsi_command::execute_operation, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Illegal format");

    controller->SetCdbByte(1, 0b001);
    Dispatch(*services, scsi_command::execute_operation, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Illegal length");

    controller->SetCdbByte(8, 1);
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::execute_operation));
}

TEST(HostServicesTest, ReceiveOperationResults)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    Dispatch(*services, scsi_command::receive_operation_results, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Illegal format");

    controller->SetCdbByte(1, 0b111);
    Dispatch(*services, scsi_command::receive_operation_results, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Illegal format");

    controller->SetCdbByte(1, 0b010);
    Dispatch(*services, scsi_command::receive_operation_results, sense_key::aborted_command,
        asc::host_services_receive_operation_results, "No matching initiator ID");
}

TEST(HostServicesTest, ModeSense6)
{
    auto [controller, services] = CreateDevice(SCHS);

    Dispatch(*services, scsi_command::mode_sense_6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    Dispatch(*services, scsi_command::mode_sense_6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::mode_sense_6));
    auto &buffer = controller->GetBuffer();
    // Major version 1
    EXPECT_EQ(0x01, buffer[6]);
    // Minor version 0
    EXPECT_EQ(0x00, buffer[7]);
    // Year
    EXPECT_NE(0x00, buffer[8]);
    // Day
    EXPECT_NE(0x00, buffer[10]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 2);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::mode_sense_6));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x01, buffer[0]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    controller->SetCdbByte(3, 0x01);
    Dispatch(*services, scsi_command::mode_sense_6, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Subpages are not supported");
}

TEST(HostServicesTest, ModeSense10)
{
    auto [controller, services] = CreateDevice(SCHS);

    Dispatch(*services, scsi_command::mode_sense_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    Dispatch(*services, scsi_command::mode_sense_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::mode_sense_10));
    auto &buffer = controller->GetBuffer();
    // Major version 1
    EXPECT_EQ(0x01, buffer[10]);
    // Minor version 0
    EXPECT_EQ(0x00, buffer[11]);
    // Year
    EXPECT_NE(0x00, buffer[12]);
    // Day
    EXPECT_NE(0x00, buffer[14]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 4);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(*services, scsi_command::mode_sense_10));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x02, buffer[1]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    controller->SetCdbByte(3, 0x01);
    Dispatch(*services, scsi_command::mode_sense_10, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "Subpages are not supported");
}

TEST(HostServicesTest, SetUpModePages)
{
    MockHostServices services(0);
    map<int, vector<byte>> pages;

    // Non changeable
    services.SetUpModePages(pages, 0x3f, false);
    ValidateModePages(pages);

    // Changeable
    pages.clear();
    services.SetUpModePages(pages, 0x3f, true);
    ValidateModePages(pages);
}

TEST(HostServicesTest, WriteData)
{
    auto [controller, services] = CreateDevice(SCHS);
    array<uint8_t, 1> buf = { };

    controller->SetCdbByte(0, static_cast<int>(scsi_command::test_unit_ready));
    EXPECT_THROW(services->WriteData(controller->GetCdb(), buf,0, 0), scsi_exception)<< "Illegal command";

    controller->SetCdbByte(0, static_cast<int>(scsi_command::execute_operation));
    EXPECT_NO_THROW(services->WriteData(controller->GetCdb(), buf, 0, 0));

    controller->SetCdbByte(0, static_cast<int>(scsi_command::execute_operation));
    controller->SetCdbByte(8, 1);
    EXPECT_THROW(services->WriteData(controller->GetCdb(), buf, 0,0), scsi_exception)<< "protobuf data are invalid";
}
