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
    EXPECT_TRUE(device->Init( { }));
    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

TEST(PrimaryDeviceTest, SetScsiLevel)
{
    MockPrimaryDevice device(0);

    EXPECT_EQ(scsi_level::scsi_2, device.GetScsiLevel());

    EXPECT_FALSE(device.SetScsiLevel(scsi_level::none));
    EXPECT_FALSE(device.SetScsiLevel(static_cast<scsi_level>(9)));

    EXPECT_TRUE(device.SetScsiLevel(scsi_level::scsi_1_ccs));
    EXPECT_EQ(scsi_level::scsi_1_ccs, device.GetScsiLevel());
    EXPECT_TRUE(device.SetScsiLevel(scsi_level::spc_6));
    EXPECT_EQ(scsi_level::spc_6, device.GetScsiLevel());
}

TEST(PrimaryDeviceTest, Status)
{
    MockPrimaryDevice device(0);

    device.SetStatus(sense_key::illegal_request, asc::parameter_list_length_error);
    EXPECT_EQ(sense_key::illegal_request, device.GetSenseKey());
    EXPECT_EQ(asc::parameter_list_length_error, device.GetAsc());
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
    EXPECT_NO_THROW(device->Dispatch(scsi_command::reserve6));
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
    EXPECT_NO_THROW(device->Dispatch(scsi_command::reserve6));
    EXPECT_TRUE(device->CheckReservation(0)) << "Device must not be reserved for initiator ID 0";
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";
    EXPECT_FALSE(device->CheckReservation(-1)) << "Device must be reserved for unknown initiator";
    controller->SetCdbByte(0, static_cast<int>(scsi_command::inquiry));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for INQUIRY";
    controller->SetCdbByte(0, static_cast<int>(scsi_command::request_sense));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for REQUEST SENSE";
    controller->SetCdbByte(0, static_cast<int>(scsi_command::release6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved for RELEASE (6)";

    controller->SetCdbByte(0, static_cast<int>(scsi_command::prevent_allow_medium_removal));
    EXPECT_TRUE(device->CheckReservation(1))
        << "Device must not be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit not set";
    controller->SetCdbByte(4, 0x01);
    EXPECT_FALSE(device->CheckReservation(1))
        << "Device must be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit set";
}

TEST(PrimaryDeviceTest, ReserveReleaseUnit)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_NO_THROW(device->Dispatch(scsi_command::reserve6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for initiator ID 1";

    EXPECT_NO_THROW(device->Dispatch(scsi_command::release6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for initiator ID 1";

    EXPECT_NO_THROW(device->Dispatch(scsi_command::reserve6));
    EXPECT_FALSE(device->CheckReservation(1)) << "Device must be reserved for unknown initiator";

    EXPECT_NO_THROW(device->Dispatch(scsi_command::release6));
    EXPECT_TRUE(device->CheckReservation(1)) << "Device must not be reserved anymore for unknown initiator";
}

TEST(PrimaryDeviceTest, DiscardReservation)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_NO_THROW(device->Dispatch(scsi_command::reserve6));
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

    EXPECT_THROW(device.ModeSelect( { }, { }, 0), scsi_exception);
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
    TestShared::Dispatch(*device, scsi_command::test_unit_ready, sense_key::unit_attention, asc::power_on_or_reset);

    device->SetReset(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::test_unit_ready, sense_key::unit_attention,
        asc::not_ready_to_ready_change);

    device->SetReset(true);
    device->SetAttn(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::test_unit_ready, sense_key::unit_attention, asc::power_on_or_reset);

    device->SetReset(false);
    device->SetAttn(true);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::test_unit_ready, sense_key::unit_attention,
        asc::not_ready_to_ready_change);

    device->SetAttn(false);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::test_unit_ready, sense_key::not_ready, asc::medium_not_present);

    device->SetReady(true);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::test_unit_ready));
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(PrimaryDeviceTest, Inquiry)
{
    auto [controller, device] = CreatePrimaryDevice();
    // Required by old clang++ compilers
    auto d = device;

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);

    ON_CALL(*d, InquiryInternal()).WillByDefault([&d]() {
        return d->HandleInquiry(device_type::processor, false);
    });
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(1));
    EXPECT_NO_THROW(device->Dispatch(scsi_command::inquiry));
    EXPECT_EQ(0x7f, controller->GetBuffer()[0]) << "Invalid LUN was not reported";
    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(0));

    EXPECT_FALSE(controller->AddDevice(make_shared<MockPrimaryDevice>(0))) << "Duplicate LUN was not rejected";
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    device->SetScsiLevel(scsi_level::spc_3);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::inquiry));
    EXPECT_EQ(device_type::processor, (device_type )controller->GetBuffer()[0]);
    EXPECT_EQ(0x00, controller->GetBuffer()[1]) << "Device was not reported as non-removable";
    EXPECT_EQ(scsi_level::spc_3, (scsi_level)controller->GetBuffer()[2]) << "Wrong SCSI level";
    EXPECT_EQ(scsi_level::scsi_2, (scsi_level)controller->GetBuffer()[3]) << "Wrong response level";
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

    ON_CALL(*d, InquiryInternal()).WillByDefault([&d]() {
        return d->HandleInquiry(device_type::direct_access, true);
    });
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    device->SetScsiLevel(scsi_level::scsi_1_ccs);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::inquiry));
    EXPECT_EQ(device_type::direct_access, (device_type )controller->GetBuffer()[0]);
    EXPECT_EQ(0x80, controller->GetBuffer()[1]) << "Device was not reported as removable";
    EXPECT_EQ(scsi_level::scsi_1_ccs, (scsi_level)controller->GetBuffer()[2]) << "Wrong SCSI level";
    EXPECT_EQ(scsi_level::scsi_1_ccs, (scsi_level)controller->GetBuffer()[3]) << "Wrong response level";
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

    controller->SetCdbByte(1, 0x01);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::inquiry, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "EVPD bit is not supported");

    controller->SetCdbByte(2, 0x01);
    EXPECT_CALL(*controller, DataIn).Times(0);
    TestShared::Dispatch(*device, scsi_command::inquiry, sense_key::illegal_request, asc::invalid_field_in_cdb,
        "PAGE CODE field is not supported");

    controller->SetCdbByte(1, 0);
    controller->SetCdbByte(2, 0);
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*device, InquiryInternal);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::inquiry));
    EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";
    EXPECT_EQ(1, controller->GetCurrentLength()) << "Wrong ALLOCATION LENGTH handling";
}

TEST(PrimaryDeviceTest, RequestSense)
{
    auto [controller, device] = CreatePrimaryDevice();

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);

    const auto &data = controller->GetBuffer();

    device->SetReady(false);
    TestShared::Dispatch(*device, scsi_command::request_sense, sense_key::not_ready, asc::medium_not_present);

    device->SetReady(true);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x00, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(0U, GetInt32(data, 3));

    device->SetFilemark();
    EXPECT_NO_THROW(device->Dispatch(scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x80, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(ascq::filemark_detected), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));

    device->SetEom(ascq::end_of_partition_medium_detected);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x40, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(ascq::end_of_partition_medium_detected), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));

    device->SetEom(ascq::beginning_of_partition_medium_detected);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ(0x70, data[0]);
    EXPECT_EQ(0x40, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(static_cast<uint8_t>(ascq::beginning_of_partition_medium_detected), data[13]);
    EXPECT_EQ(0U, GetInt32(data, 3));

    device->SetInformation(0x12345678);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::request_sense));
    EXPECT_EQ(status_code::good, controller->GetStatus());
    EXPECT_EQ(0xf0, data[0]);
    EXPECT_EQ(0x00, data[2]);
    EXPECT_EQ(10, data[7]);
    EXPECT_EQ(0x12345678U, GetInt32(data, 3));
}

TEST(PrimaryDeviceTest, SendDiagnostic)
{
    auto [controller, device] = CreatePrimaryDevice();

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(device->Dispatch(scsi_command::send_diagnostic));
    EXPECT_EQ(status_code::good, controller->GetStatus());

    controller->SetCdbByte(3, 1);
    TestShared::Dispatch(*device, scsi_command::send_diagnostic, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "SEND DIAGNOSTIC must fail because parameter list is not supported");
    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 1);
    TestShared::Dispatch(*device, scsi_command::send_diagnostic, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "SEND DIAGNOSTIC must fail because parameter list is not supported");
}

TEST(PrimaryDeviceTest, ReportLuns)
{
    const int LUN1 = 1;
    const int LUN2 = 4;

    auto controller = make_shared<MockAbstractController>(0);
    auto device1 = make_shared<MockPrimaryDevice>(LUN1);
    auto device2 = make_shared<MockPrimaryDevice>(LUN2);
    EXPECT_TRUE(device1->Init( { }));
    EXPECT_TRUE(device2->Init( { }));

    controller->AddDevice(device1);
    EXPECT_TRUE(controller->GetDeviceForLun(LUN1));
    controller->AddDevice(device2);
    EXPECT_TRUE(controller->GetDeviceForLun(LUN2));

    // ALLOCATION LENGTH
    controller->SetCdbByte(9, 255);

    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(device1->Dispatch(scsi_command::report_luns));
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
    TestShared::Dispatch(*device1, scsi_command::report_luns, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Only SELECT REPORT mode 0 is supported");
}

TEST(PrimaryDeviceTest, Dispatch)
{
    MockPrimaryDevice device(0);

    TestShared::Dispatch(device, static_cast<scsi_command>(0x1f), sense_key::illegal_request,
        asc::invalid_command_operation_code, "Unsupported SCSI command");
}

TEST(PrimaryDeviceTest, Init)
{
    MockPrimaryDevice device(0);

    EXPECT_TRUE(device.Init( {})) << "Initialization of primary device must not fail";
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
