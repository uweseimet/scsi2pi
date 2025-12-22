//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
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
    EXPECT_FALSE(services.SupportsImageFile());
    EXPECT_FALSE(services.SupportsParams());
    EXPECT_FALSE(services.IsProtectable());
    EXPECT_FALSE(services.IsProtected());
    EXPECT_FALSE(services.IsReadOnly());
    EXPECT_FALSE(services.IsRemovable());
    EXPECT_FALSE(services.IsRemoved());
    EXPECT_FALSE(services.IsLocked());
    EXPECT_FALSE(services.IsStoppable());
    EXPECT_FALSE(services.IsStopped());

    const auto& [vendor, product, revision] = services.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("Host Services", product);
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST(HostServicesTest, GetIdentifier)
{
    HostServices services(0);

    EXPECT_EQ("Host Services", services.GetIdentifier());
}

TEST(HostServicesTest, TestUnitReady)
{
    auto [controller, services] = CreateDevice(SCHS);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::TEST_UNIT_READY));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(HostServicesTest, Inquiry)
{
    TestShared::Inquiry(SCHS, DeviceType::PROCESSOR, ScsiLevel::SPC_3, "SCSI2Pi Host Services   ", 0x1f, false);
}

TEST(HostServicesTest, StartStopUnit)
{
    auto [controller, services] = CreateDevice(SCHS);

    // STOP
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::START_STOP));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());

    // LOAD
    controller->SetCdbByte(4, 0x02);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::START_STOP));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());

    // UNLOAD
    controller->SetCdbByte(4, 0x03);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::START_STOP));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());

    // START
    controller->SetCdbByte(4, 0x01);
    Dispatch(services, ScsiCommand::START_STOP, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
}

TEST(HostServicesTest, ExecuteOperation)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    Dispatch(services, ScsiCommand::EXECUTE_OPERATION, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal format");

    controller->SetCdbByte(1, 0b111);
    Dispatch(services, ScsiCommand::EXECUTE_OPERATION, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal format");

    controller->SetCdbByte(1, 0b001);
    Dispatch(services, ScsiCommand::EXECUTE_OPERATION, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal length");

    controller->SetCdbByte(8, 1);
    controller->SetCdbByte(1, 0b001);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::EXECUTE_OPERATION));
}

TEST(HostServicesTest, ReceiveOperationResults)
{
    auto [controller, services] = CreateDevice(SCHS);

    controller->SetCdbByte(1, 0b000);
    Dispatch(services, ScsiCommand::RECEIVE_OPERATION_RESULTS, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal format");

    controller->SetCdbByte(1, 0b111);
    Dispatch(services, ScsiCommand::RECEIVE_OPERATION_RESULTS, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal format");

    controller->SetCdbByte(1, 0b11000);
    Dispatch(services, ScsiCommand::RECEIVE_OPERATION_RESULTS, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Illegal format");

    controller->SetCdbByte(1, 0b010);
    Dispatch(services, ScsiCommand::RECEIVE_OPERATION_RESULTS, SenseKey::ILLEGAL_REQUEST,
        Asc::DATA_CURRENTLY_UNAVAILABLE, "No matching initiator ID");
}

TEST(HostServicesTest, ModeSense6)
{
    auto [controller, services] = CreateDevice(SCHS);

    Dispatch(services, ScsiCommand::MODE_SENSE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    Dispatch(services, ScsiCommand::MODE_SENSE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::MODE_SENSE_6));
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
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::MODE_SENSE_6));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x01, buffer[0]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    controller->SetCdbByte(3, 0x01);
    Dispatch(services, ScsiCommand::MODE_SENSE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Subpages are not supported");
}

TEST(HostServicesTest, ModeSense10)
{
    auto [controller, services] = CreateDevice(SCHS);

    Dispatch(services, ScsiCommand::MODE_SENSE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Unsupported mode page was returned");

    controller->SetCdbByte(2, 0x20);
    Dispatch(services, ScsiCommand::MODE_SENSE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Block descriptors are not supported");

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    // ALLOCATION LENGTH
    controller->SetCdbByte(8, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::MODE_SENSE_10));
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
    EXPECT_NO_THROW(Dispatch(services, ScsiCommand::MODE_SENSE_10));
    buffer = controller->GetBuffer();
    EXPECT_EQ(0x02, buffer[1]);

    controller->SetCdbByte(1, 0x08);
    controller->SetCdbByte(2, 0x20);
    controller->SetCdbByte(3, 0x01);
    Dispatch(services, ScsiCommand::MODE_SENSE_10, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
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
    const array<const uint8_t, 1> buf = { };

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::TEST_UNIT_READY));
    EXPECT_THROW(services->WriteData(controller->GetCdb(), buf, 0), ScsiException)<< "Illegal command";

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::EXECUTE_OPERATION));
    EXPECT_NO_THROW(services->WriteData(controller->GetCdb(), buf, 0));

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::EXECUTE_OPERATION));
    controller->SetCdbByte(8, 1);
    EXPECT_THROW(services->WriteData(controller->GetCdb(), buf, 0), ScsiException)<< "protobuf data are invalid";
}
