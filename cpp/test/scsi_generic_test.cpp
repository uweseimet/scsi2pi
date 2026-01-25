//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "test_shared.h"
#include "devices/scsi_generic.h"
#include "shared/s2p_exceptions.h"

using namespace testing;

TEST(ScsiGenericTest, Device_Defaults)
{
    ScsiGeneric device(0, "");

    EXPECT_EQ(SCSG, device.GetType());
    EXPECT_FALSE(device.SupportsImageFile());
    EXPECT_TRUE(device.SupportsParams());
    EXPECT_FALSE(device.IsProtectable());
    EXPECT_FALSE(device.IsProtected());
    EXPECT_FALSE(device.IsReadOnly());
    EXPECT_FALSE(device.IsRemovable());
    EXPECT_FALSE(device.IsRemoved());
    EXPECT_FALSE(device.IsLocked());
    EXPECT_FALSE(device.IsStoppable());
    EXPECT_FALSE(device.IsStopped());

    const auto& [vendor, product, revision] = device.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("", product);
    EXPECT_EQ(testing::TestShared::GetVersion(), revision);
}

TEST(ScsiGenericTest, GetIdentifier)
{
    ScsiGeneric device(0, "");

    EXPECT_EQ(" (SCSI2Pi                 " + testing::TestShared::GetVersion() + ")", device.GetIdentifier());
}

TEST(ScsiGenericTest, SetUp)
{
    ScsiGeneric device1(0, "");
    EXPECT_NE("", device1.SetUp());

    ScsiGeneric device2(0, "/dev/null");
    EXPECT_NE("", device2.SetUp());

    ScsiGeneric device3(0, "/dev/sg0123456789");
    EXPECT_NE("", device3.SetUp());
}

TEST(ScsiGenericTest, Dispatch)
{
    auto [controller, d] = CreateDevice(SCSG);
    // Work-around for an issue with old compilers
    auto device = d;

    EXPECT_THAT([&] { device->Dispatch(static_cast<ScsiCommand>(0x1f)); },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, SenseKey::ILLEGAL_REQUEST),
            Property(&ScsiException::GetAsc, Asc::INVALID_COMMAND_OPERATION_CODE))));

    EXPECT_THAT([&] { device->Dispatch(ScsiCommand::TEST_UNIT_READY) ; },
        Throws<ScsiException>(AllOf(
        Property(&ScsiException::GetSenseKey, SenseKey::ABORTED_COMMAND),
        Property(&ScsiException::GetAsc, Asc::READ_ERROR))));

    EXPECT_THAT([&] { device->Dispatch(ScsiCommand::READ_6) ; },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, SenseKey::ABORTED_COMMAND),
            Property(&ScsiException::GetAsc, Asc::READ_ERROR))));

    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(device->Dispatch(ScsiCommand::WRITE_6));

    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(device->Dispatch(ScsiCommand::FORMAT_UNIT));

    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(1));
    EXPECT_THAT([&] { device->Dispatch(ScsiCommand::FORMAT_UNIT) ; },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, SenseKey::ILLEGAL_REQUEST),
            Property(&ScsiException::GetAsc, Asc::LOGICAL_UNIT_NOT_SUPPORTED))));
}
