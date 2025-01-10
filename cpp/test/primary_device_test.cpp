//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "base/device_factory.h"
#include "shared/s2p_exceptions.h"

using namespace memory_util;

pair<shared_ptr<MockAbstractController>, shared_ptr<MockPrimaryDevice>> CreatePrimaryDevice(int id = 0)
{
    auto controller = make_shared<NiceMock<MockAbstractController>>(id);
    auto device = make_shared<MockPrimaryDevice>(0);
    EXPECT_EQ("", device->Init());
    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

TEST(PrimaryDeviceTest, ProductData)
{
    MockPrimaryDevice device(0);

    EXPECT_TRUE(device.SetProductData( { "", "", "" }, true).empty());
    EXPECT_FALSE(device.SetProductData( { "123456789", "", "" }, true).empty());
    EXPECT_TRUE(device.SetProductData( { "12345678", "", "" }, true).empty());
    EXPECT_EQ("12345678", device.GetProductData().vendor);
    EXPECT_TRUE(device.SetProductData( { " 12345678 ", "", "" }, true).empty());
    EXPECT_EQ("12345678", device.GetProductData().vendor);

    EXPECT_FALSE(device.SetProductData( { "", "12345678901234567", "" }, true).empty());
    EXPECT_TRUE(device.SetProductData( { "", "1234567890123456", "" }, true).empty());
    EXPECT_EQ("1234567890123456", device.GetProductData().product);
    EXPECT_TRUE(device.SetProductData( { "", " 1234567890123456 ", "" }, true).empty());
    EXPECT_EQ("1234567890123456", device.GetProductData().product);
    EXPECT_TRUE(device.SetProductData( { "", "xyz", "" }, false).empty());
    EXPECT_EQ("1234567890123456", device.GetProductData().product)
    << "Changing vital product data is not SCSI compliant";

    EXPECT_FALSE(device.SetProductData( { "", "", "12345" }, true).empty());
    EXPECT_TRUE(device.SetProductData( { "", "", "1234" }, true).empty());
    EXPECT_EQ("1234", device.GetProductData().revision);
    EXPECT_TRUE(device.SetProductData( { "", "", " 1234 " }, true).empty());
    EXPECT_EQ("1234", device.GetProductData().revision);
}

TEST(PrimaryDeviceTest, GetPaddedName)
{
    MockPrimaryDevice device(0);

    device.SetProductData( { "V", "P", "R" }, true);
    EXPECT_EQ("V       P               R   ", device.GetPaddedName());
}

TEST(PrimaryDeviceTest, SetScsiLevel)
{
    MockPrimaryDevice device(0);

    EXPECT_EQ(ScsiLevel::NONE, device.GetScsiLevel());

    EXPECT_TRUE(device.SetScsiLevel(ScsiLevel::NONE));
    EXPECT_FALSE(device.SetScsiLevel(static_cast<ScsiLevel>(10)));

    EXPECT_TRUE(device.SetScsiLevel(ScsiLevel::SCSI_1_CCS));
    EXPECT_EQ(ScsiLevel::SCSI_1_CCS, device.GetScsiLevel());
    EXPECT_TRUE(device.SetScsiLevel(ScsiLevel::SPC_7));
    EXPECT_EQ(ScsiLevel::SPC_7, device.GetScsiLevel());
}

TEST(PrimaryDeviceTest, SetResponseDataFormat)
{
    MockPrimaryDevice device(0);

    EXPECT_FALSE(device.SetResponseDataFormat(ScsiLevel::NONE));
    EXPECT_TRUE(device.SetResponseDataFormat(ScsiLevel::SCSI_1_CCS));
    EXPECT_TRUE(device.SetResponseDataFormat(ScsiLevel::SCSI_2));
    EXPECT_FALSE(device.SetResponseDataFormat(ScsiLevel::SPC));
}

TEST(PrimaryDeviceTest, Status)
{
    MockPrimaryDevice device(0);

    device.SetStatus(SenseKey::ILLEGAL_REQUEST, Asc::PARAMETER_LIST_LENGTH_ERROR);
    EXPECT_EQ(SenseKey::ILLEGAL_REQUEST, device.GetSenseKey());
    EXPECT_EQ(Asc::PARAMETER_LIST_LENGTH_ERROR, device.GetAsc());
}

TEST(PrimaryDeviceTest, GetId)
{
    const int ID = 5;

    auto [controller, device] = CreatePrimaryDevice(ID);

    EXPECT_EQ(ID, device->GetId());
}

TEST(PrimaryDeviceTest, StatusPhase)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_CALL(*controller, Status);
    device->StatusPhase();
}

TEST(PrimaryDeviceTest, DataInPhase)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_CALL(*controller, DataIn);
    device->DataInPhase(123);
    EXPECT_EQ(123, controller->GetCurrentLength());
}

TEST(PrimaryDeviceTest, DataOutPhase)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_CALL(*controller, DataOut);
    device->DataOutPhase(456);
    EXPECT_EQ(456, controller->GetCurrentLength());
}

TEST(PrimaryDeviceTest, Reset)
{
    auto [controller, device] = CreatePrimaryDevice();

    device->SetLocked(true);
    device->SetAttn(true);
    device->SetReset(true);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";
    device->Reset();
    EXPECT_FALSE(device->IsLocked());
    EXPECT_FALSE(device->IsAttn());
    EXPECT_FALSE(device->IsReset());
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for initiator ID 1";
}

TEST(PrimaryDeviceTest, CheckReservation)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_TRUE(device->CheckReservation(0)) << "Device must not be reserved for initiator ID 0";

    controller->ProcessOnController(0);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_TRUE(device->CheckReservation(0)) << "Device must not be reserved for initiator ID 0";
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";
    EXPECT_FALSE(device->CheckReservation(-1)) << "Device must be reserved for unknown initiator";
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::INQUIRY));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for INQUIRY";
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::REQUEST_SENSE));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for REQUEST SENSE";
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::RELEASE_RELEASE_ELEMENT_6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for RELEASE (6)";

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL));
    EXPECT_TRUE(device->CheckReservation(1))
        << "Device must not be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit not set";
    controller->SetCdbByte(4, 0x01);
    EXPECT_FALSE(device->CheckReservation(1))
        << "Device must be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit set";
}

TEST(PrimaryDeviceTest, ReserveRelease)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";

    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RELEASE_RELEASE_ELEMENT_6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for initiator ID 1";

    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for unknown initiator";

    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RELEASE_RELEASE_ELEMENT_6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for unknown initiator";
}

TEST(PrimaryDeviceTest, DiscardReservation)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";
    EXPECT_NO_THROW(device->DiscardReservation());
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for initiator ID 1";
}

TEST(PrimaryDeviceTest, ReadData)
{
    MockPrimaryDevice device(0);

    EXPECT_EQ(0, device.ReadData( { }));
}

TEST(PrimaryDeviceTest, ModeSelect)
{
    MockPrimaryDevice device(0);

    EXPECT_THROW(device.ModeSelect( { }, { }, 0, 0), ScsiException);
}

TEST(PrimaryDeviceTest, ModeSense6)
{
    MockPrimaryDevice device(0);
    vector<uint8_t> buf;

    EXPECT_EQ(0, device.ModeSense6( { }, buf));
}

TEST(PrimaryDeviceTest, ModeSense10)
{
    MockPrimaryDevice device(0);
    vector<uint8_t> buf;

    EXPECT_EQ(0, device.ModeSense10( { }, buf));
}

TEST(PrimaryDeviceTest, SetUpModePages)
{
    MockPrimaryDevice device(0);
    map<int, vector<byte>> pages;

    // Non changeable
    device.SetUpModePages(pages, 0x3f, false);
    EXPECT_TRUE(pages.empty());

    // Changeable
    device.SetUpModePages(pages, 0x3f, true);
    EXPECT_TRUE(pages.empty());
}

TEST(PrimaryDeviceTest, TestUnitReady)
{
    auto [controller, device] = CreatePrimaryDevice();

    device->SetReset(true);
    device->SetAttn(true);
    device->SetReady(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::TEST_UNIT_READY, SenseKey::UNIT_ATTENTION, Asc::POWER_ON_OR_RESET);

    device->SetReset(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::TEST_UNIT_READY, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_CHANGE);

    device->SetReset(true);
    device->SetAttn(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::TEST_UNIT_READY, SenseKey::UNIT_ATTENTION, Asc::POWER_ON_OR_RESET);
    device->SetReset(false);
    device->SetAttn(true);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::TEST_UNIT_READY, SenseKey::UNIT_ATTENTION, Asc::NOT_READY_TO_READY_CHANGE);

    device->SetAttn(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::TEST_UNIT_READY, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    device->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::TEST_UNIT_READY));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(PrimaryDeviceTest, Inquiry)
{
    auto [controller, device] = CreatePrimaryDevice();
    // Required by old clang++ compilers
    auto d = device;

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    ON_CALL(*d, InquiryInternal()).WillByDefault([&d]() {
        return d->HandleInquiry(DeviceType::PROCESSOR, false);
    });
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(1));
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::INQUIRY));
    EXPECT_EQ(0x7f, controller->GetBuffer()[0]) << "Invalid LUN was not reported";
    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(0));

    EXPECT_FALSE(controller->AddDevice(make_shared<MockPrimaryDevice>(0))) << "Duplicate LUN was not rejected";
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    device->SetScsiLevel(ScsiLevel::SPC_3);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::INQUIRY));
    EXPECT_EQ(DeviceType::PROCESSOR, (DeviceType )controller->GetBuffer()[0]);
    EXPECT_EQ(0x00, controller->GetBuffer()[1]) << "Device was not reported as non-removable";
    EXPECT_EQ(ScsiLevel::SPC_3, (ScsiLevel)controller->GetBuffer()[2]) << "Wrong SCSI level";
    EXPECT_EQ(ScsiLevel::SCSI_2, (ScsiLevel)controller->GetBuffer()[3]) << "Wrong response level";
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    ON_CALL(*d, InquiryInternal()).WillByDefault([&d]() {
        return d->HandleInquiry(DeviceType::DIRECT_ACCESS, true);
    });
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    device->SetScsiLevel(ScsiLevel::SCSI_1_CCS);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::INQUIRY));
    EXPECT_EQ(DeviceType::DIRECT_ACCESS, (DeviceType )controller->GetBuffer()[0]);
    EXPECT_EQ(0x80, controller->GetBuffer()[1]) << "Device was not reported as removable";
    EXPECT_EQ(ScsiLevel::SCSI_1_CCS, (ScsiLevel)controller->GetBuffer()[2]) << "Wrong SCSI level";
    EXPECT_EQ(ScsiLevel::SCSI_1_CCS, (ScsiLevel)controller->GetBuffer()[3]) << "Wrong response level";
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

    controller->SetCdbByte(1, 0x01);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::INQUIRY, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "EVPD bit is not supported");

    controller->SetCdbByte(2, 0x01);
    EXPECT_CALL(*controller, DataIn).Times(0);
    Dispatch(device, ScsiCommand::INQUIRY, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "PAGE CODE field is not supported");

    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(2, 0);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::INQUIRY));
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";
    EXPECT_EQ(1, controller->GetCurrentLength()) << "Wrong ALLOCATION LENGTH handling";
}

TEST(PrimaryDeviceTest, RequestSense)
{
    auto [controller, device] = CreatePrimaryDevice();

    const auto &data = controller->GetBuffer();

    // DESC
    controller->SetCdbByte(1, 0x01);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    Dispatch(device, ScsiCommand::REQUEST_SENSE, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);

    device->SetReady(false);
    Dispatch(device, ScsiCommand::REQUEST_SENSE, SenseKey::NOT_READY, Asc::MEDIUM_NOT_PRESENT);

    device->SetReady(true);
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x00, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(0U, GetInt32(data, 3));
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetFilemark();
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x80, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(Ascq::FILEMARK_DETECTED), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetEom(Ascq::END_OF_PARTITION_MEDIUM_DETECTED);
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x40, data[2]) << "EOM must be set";
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(Ascq::END_OF_PARTITION_MEDIUM_DETECTED), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetEom(Ascq::BEGINNING_OF_PARTITION_MEDIUM_DETECTED);
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x40, data[2]) << "EOM must be set";
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(Ascq::BEGINNING_OF_PARTITION_MEDIUM_DETECTED), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetIli();
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x20, data[2]) << "ILI must be set";
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetInformation(0x12345678);
    RequestSense(controller, device);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0xf0, data[0]);
    EXPECT_EQ(0x00, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(0x000000U, GetInt32(data, 14));

    device->SetScsiLevel(ScsiLevel::SCSI_1_CCS);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 0);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::REQUEST_SENSE));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
    EXPECT_EQ(0x00, data[0]);
}

TEST(PrimaryDeviceTest, SendDiagnostic)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(device, ScsiCommand::SEND_DIAGNOSTIC));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());

    controller->SetCdbByte(3, 1);
    Dispatch(device, ScsiCommand::SEND_DIAGNOSTIC, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "SEND DIAGNOSTIC must fail because parameter list is not supported");
    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 1);
    Dispatch(device, ScsiCommand::SEND_DIAGNOSTIC, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "SEND DIAGNOSTIC must fail because parameter list is not supported");
}

TEST(PrimaryDeviceTest, ReportLuns)
{
    const int LUN1 = 1;
    const int LUN2 = 4;

    auto controller = make_shared<MockAbstractController>(0);
    auto device1 = make_shared<MockPrimaryDevice>(LUN1);
    auto device2 = make_shared<MockPrimaryDevice>(LUN2);
    EXPECT_EQ("", device1->Init());
    EXPECT_EQ("", device2->Init());

    controller->AddDevice(device1);
    EXPECT_TRUE(controller->GetDeviceForLun(LUN1));
    controller->AddDevice(device2);
    EXPECT_TRUE(controller->GetDeviceForLun(LUN2));

    // ALLOCATION LENGTH
    controller->SetCdbByte(9, 255);

    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(device1, ScsiCommand::REPORT_LUNS));
    span<uint8_t> buffer = controller->GetBuffer();
    EXPECT_EQ(0, GetInt16(buffer, 0)) << "Wrong data length";
    EXPECT_EQ(16, GetInt16(buffer, 2)) << "Wrong data length";
    EXPECT_EQ(0, GetInt16(buffer, 8)) << "Wrong LUN1 number";
    EXPECT_EQ(0, GetInt16(buffer, 10)) << "Wrong LUN1 number";
    EXPECT_EQ(0, GetInt16(buffer, 12)) << "Wrong LUN1 number";
    EXPECT_EQ(LUN1, GetInt16(buffer, 14)) << "Wrong LUN1 number";
    EXPECT_EQ(0, GetInt16(buffer, 16)) << "Wrong LUN2 number";
    EXPECT_EQ(0, GetInt16(buffer, 18)) << "Wrong LUN2 number";
    EXPECT_EQ(0, GetInt16(buffer, 20)) << "Wrong LUN2 number";
    EXPECT_EQ(LUN2, GetInt16(buffer, 22)) << "Wrong LUN2 number";

    controller->SetCdbByte(2, 0x01);
    Dispatch(device1, ScsiCommand::REPORT_LUNS, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Only SELECT REPORT mode 0 is supported");
}

TEST(PrimaryDeviceTest, Dispatch)
{
    Dispatch(make_shared<MockPrimaryDevice>(0), static_cast<ScsiCommand>(0x1f), SenseKey::ILLEGAL_REQUEST,
        Asc::INVALID_COMMAND_OPERATION_CODE, "Unsupported SCSI command");
}

TEST(PrimaryDeviceTest, Init)
{
    MockPrimaryDevice device(0);

    EXPECT_EQ("", device.Init()) << "Initialization of primary device must not fail";
}

TEST(PrimaryDeviceTest, GetDelayAfterBytes)
{
    MockPrimaryDevice device(0);

    EXPECT_EQ(-1, device.GetDelayAfterBytes());
}

TEST(PrimaryDeviceTest, GetStatistics)
{
    MockPrimaryDevice device(0);

    EXPECT_TRUE(device.GetStatistics().empty());
}
