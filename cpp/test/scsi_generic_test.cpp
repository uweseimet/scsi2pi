//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "test_shared.h"
#include "devices/scsi_generic.h"
#include "shared/s2p_exceptions.h"

using namespace s2p_test;

TEST(ScsiGenericTest, Device_Defaults)
{
    ScsiGeneric device(0, "");

    EXPECT_EQ(SCSG, device.GetType());
    EXPECT_TRUE(device.SupportsFile());
    EXPECT_FALSE(device.SupportsParams());
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
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST(ScsiGenericTest, GetIdentifier)
{
    ScsiGeneric device(0, "");

    EXPECT_EQ(" (SCSI2Pi                 " + TestShared::GetVersion() + ")", device.GetIdentifier());
}

TEST(ScsiGenericTest, SetUp)
{
    ScsiGeneric device1(0, "");
    EXPECT_NE("", device1.SetUp());

    ScsiGeneric device2(0, "/dev/null");
    EXPECT_NE("", device2.SetUp());

    ScsiGeneric device3(0, "");
    param_map params;
    params["device"] = "/dev/sg0123456789";
    device3.SetParams(params);
    EXPECT_NE("", device3.SetUp());
}

TEST(ScsiGenericTest, Dispatch)
{
    auto [controller, d] = CreateDevice(SCSG);
    // Work-around for an issue with old compilers
    auto device = d;

    EXPECT_THAT([&] { device->Dispatch(static_cast<ScsiCommand>(0x1f)); },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, ILLEGAL_REQUEST),
            Property(&ScsiException::GetAsc, INVALID_COMMAND_OPERATION_CODE))));

    EXPECT_THAT([&] { device->Dispatch(TEST_UNIT_READY) ; },
        Throws<ScsiException>(AllOf(
        Property(&ScsiException::GetSenseKey, ABORTED_COMMAND),
        Property(&ScsiException::GetAsc, READ_ERROR))));

    EXPECT_THAT([&] { device->Dispatch(READ_6) ; },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, ABORTED_COMMAND),
            Property(&ScsiException::GetAsc, READ_ERROR))));

    EXPECT_CALL(*controller, DataOut);
    device->Dispatch(WRITE_6);

    EXPECT_CALL(*controller, DataOut);
    device->Dispatch(FORMAT);

    ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(1));
    EXPECT_THAT([&] { device->Dispatch(FORMAT) ; },
        Throws<ScsiException>(AllOf(
            Property(&ScsiException::GetSenseKey, ILLEGAL_REQUEST),
            Property(&ScsiException::GetAsc, LOGICAL_UNIT_NOT_SUPPORTED))));
}
