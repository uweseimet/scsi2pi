//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

using namespace std;

TEST(HostServicesTest, DeviceDefaults)
{
    DeviceFactory device_factory;

    auto device = device_factory.CreateDevice(UNDEFINED, 0, "services");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCHS, device->GetType());
    EXPECT_FALSE(device->SupportsFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_FALSE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_FALSE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_FALSE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("SCSI2Pi", device->GetVendor());
    EXPECT_EQ("Host Services", device->GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), device->GetRevision());
}

void HostServices_SetUpModePages(map<int, vector<byte>> &pages)
{
    EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
    EXPECT_EQ(10, pages[32].size());
}

TEST(HostServicesTest, TestUnitReady)
{
    auto [controller, services] = CreateDevice(SCHS);

    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::cmd_test_unit_ready);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(HostServicesTest, Inquiry)
{
    TestShared::Inquiry(SCHS, device_type::processor, scsi_level::spc_3, "SCSI2Pi Host Services   ", 0x1f, false);
}

TEST(HostServicesTest, StartStopUnit)
{
    auto [controller, services] = CreateDevice(SCHS);
    // Required by the bullseye clang++ compiler
    auto s = services;

    // STOP
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::cmd_start_stop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // LOAD
    controller->SetCmdByte(4, 0x02);
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::cmd_start_stop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // UNLOAD
    controller->SetCmdByte(4, 0x03);
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::cmd_start_stop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // START
    controller->SetCmdByte(4, 0x01);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_start_stop)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, ExecuteOperation)
{
    auto [controller, services] = CreateDevice(SCHS);
    // Required by the bullseye clang++ compiler
    auto s = services;

    // Illegal format
    controller->SetCmdByte(1, 0b000);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_execute_operation)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));

    // Illegal format
    controller->SetCmdByte(1, 0b111);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_execute_operation)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));

    controller->SetCmdByte(1, 0b001);
    // Illegal length
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_execute_operation)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, ReceiveOperationResults)
{
    auto [controller, services] = CreateDevice(SCHS);
    // Required by the bullseye clang++ compiler
    auto s = services;

    // Illegal format
    controller->SetCmdByte(1, 0b000);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_receive_operation_results)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));

    // Illegal format
    controller->SetCmdByte(1, 0b111);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_receive_operation_results)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
            Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));

    // No matching initiator ID, command must be aborted
    controller->SetCmdByte(1, 0b010);
    EXPECT_THAT([&]
        {
            s->Dispatch(scsi_command::cmd_receive_operation_results)
            ;
        }, Throws<scsi_exception>(AllOf(
            Property(&scsi_exception::get_sense_key, sense_key::aborted_command))));
}

TEST(HostServicesTest, ModeSense6)
{
    auto [controller, services] = CreateDevice(SCHS);
    // Required by the bullseye clang++ compiler
    auto s = services;

    EXPECT_TRUE(services->Init( { }));

    EXPECT_THAT([&] {s->Dispatch(scsi_command::cmd_mode_sense6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Unsupported mode page was returned";

    controller->SetCmdByte(2, 0x20);
    EXPECT_THAT([&] {s->Dispatch(scsi_command::cmd_mode_sense6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Block descriptors are not supported";

    controller->SetCmdByte(1, 0x08);
    // ALLOCATION LENGTH
    controller->SetCmdByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::cmd_mode_sense6);
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
    controller->SetCmdByte(4, 2);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::cmd_mode_sense6);
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x02, buffer[0]);
}

TEST(HostServicesTest, ModeSense10)
{
    auto [controller, services] = CreateDevice(SCHS);
    // Required by the bullseye clang++ compiler
    auto s = services;

    EXPECT_TRUE(services->Init( { }));

    EXPECT_THAT([&] {s->Dispatch(scsi_command::cmd_mode_sense10);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Unsupported mode page was returned";

    controller->SetCmdByte(2, 0x20);
    EXPECT_THAT([&] {s->Dispatch(scsi_command::cmd_mode_sense10);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Block descriptors are not supported";

    controller->SetCmdByte(1, 0x08);
    // ALLOCATION LENGTH
    controller->SetCmdByte(8, 255);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::cmd_mode_sense10);
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
    controller->SetCmdByte(8, 2);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::cmd_mode_sense10);
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x02, buffer[1]);
}

TEST(HostServicesTest, SetUpModePages)
{
    MockHostServices services(0);
    map<int, vector<byte>> pages;

    // Non changeable
    services.SetUpModePages(pages, 0x3f, false);
    HostServices_SetUpModePages(pages);

    // Changeable
    pages.clear();
    services.SetUpModePages(pages, 0x3f, true);
    HostServices_SetUpModePages(pages);
}
