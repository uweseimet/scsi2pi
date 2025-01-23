//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(DeviceTest, GetDefaultParams)
{
    MockDevice device(0);

    const auto params = device.GetDefaultParams();
    EXPECT_TRUE(params.empty());
}

TEST(DeviceTest, Properties)
{
    const int LUN = 5;

    MockDevice device(LUN);

    EXPECT_FALSE(device.IsReady()) << "Wrong default value";
    device.SetReady(true);
    EXPECT_TRUE(device.IsReady());
    device.SetReady(false);
    EXPECT_FALSE(device.IsReady());

    EXPECT_FALSE(device.IsReset()) << "Wrong default value";
    device.SetReset(true);
    EXPECT_TRUE(device.IsReset());
    device.SetReset(false);
    EXPECT_FALSE(device.IsReset());

    EXPECT_FALSE(device.IsAttn()) << "Wrong default value";
    device.SetAttn(true);
    EXPECT_TRUE(device.IsAttn());
    device.SetAttn(false);
    EXPECT_FALSE(device.IsAttn());

    EXPECT_FALSE(device.IsReadOnly()) << "Wrong default value";
    device.SetReadOnly(true);
    EXPECT_TRUE(device.IsReadOnly());
    device.SetReadOnly(false);
    EXPECT_FALSE(device.IsReadOnly());

    EXPECT_FALSE(device.IsProtectable()) << "Wrong default value";
    device.SetProtectable(true);
    EXPECT_TRUE(device.IsProtectable());
    device.SetProtectable(false);
    EXPECT_FALSE(device.IsProtectable());

    EXPECT_FALSE(device.IsProtected()) << "Wrong default value";
    device.SetProtected(true);
    EXPECT_FALSE(device.IsProtected());
    device.SetProtectable(true);
    device.SetProtected(true);
    EXPECT_TRUE(device.IsProtected());
    device.SetProtected(false);
    EXPECT_FALSE(device.IsProtected());

    device.SetProtectable(false);
    device.SetReadOnly(true);
    device.SetProtected(true);
    EXPECT_FALSE(device.IsProtected()) << "Read-only or not protectable devices cannot be protected";
    device.SetReadOnly(false);
    device.SetProtected(true);
    EXPECT_FALSE(device.IsProtected()) << "Read-only or not protectable devices cannot be protected";

    EXPECT_FALSE(device.IsStoppable()) << "Wrong default value";
    device.SetStoppable(true);
    EXPECT_TRUE(device.IsStoppable());
    device.SetStoppable(false);
    EXPECT_FALSE(device.IsStoppable());

    EXPECT_FALSE(device.IsStopped()) << "Wrong default value";
    device.SetStopped(true);
    EXPECT_TRUE(device.IsStopped());
    device.SetStopped(false);
    EXPECT_FALSE(device.IsStopped());

    EXPECT_FALSE(device.IsRemovable()) << "Wrong default value";
    device.SetRemovable(true);
    EXPECT_TRUE(device.IsRemovable());
    device.SetRemovable(false);
    EXPECT_FALSE(device.IsRemovable());

    EXPECT_FALSE(device.IsRemoved()) << "Wrong default value";
    device.SetRemoved(true);
    EXPECT_TRUE(device.IsRemoved());
    device.SetRemoved(false);
    EXPECT_FALSE(device.IsRemoved());

    EXPECT_FALSE(device.IsLocked()) << "Wrong default value";
    device.SetLocked(true);
    EXPECT_TRUE(device.IsLocked());
    device.SetLocked(false);
    EXPECT_FALSE(device.IsLocked());

    EXPECT_FALSE(device.SupportsParams()) << "Wrong default value";
    device.SupportsParams(true);
    EXPECT_TRUE(device.SupportsParams());
    device.SupportsParams(false);
    EXPECT_FALSE(device.SupportsParams());

    EXPECT_FALSE(device.SupportsImageFile()) << "Wrong default value";

    EXPECT_EQ(LUN, device.GetLun());
}

TEST(DeviceTest, Start)
{
    MockDevice device(0);

    device.SetStopped(true);
    device.SetReady(false);
    EXPECT_FALSE(device.Start());
    EXPECT_TRUE(device.IsStopped());
    device.SetReady(true);
    EXPECT_TRUE(device.Start());
    EXPECT_FALSE(device.IsStopped());
}

TEST(DeviceTest, Stop)
{
    MockDevice device(0);

    device.SetReady(true);
    device.SetAttn(true);
    device.SetStopped(false);
    device.Stop();
    EXPECT_FALSE(device.IsReady());
    EXPECT_FALSE(device.IsAttn());
    EXPECT_TRUE(device.IsStopped());
}

TEST(DeviceTest, Eject)
{
    MockDevice device(0);

    device.SetReady(false);
    device.SetRemovable(false);
    EXPECT_FALSE(device.Eject(false));

    device.SetReady(true);
    EXPECT_FALSE(device.Eject(false));

    device.SetRemovable(true);
    device.SetLocked(true);
    EXPECT_FALSE(device.Eject(false));
    EXPECT_TRUE(device.Eject(true));

    device.SetReady(true);
    device.SetLocked(false);
    EXPECT_TRUE(device.Eject(false));
    EXPECT_FALSE(device.IsReady());
    EXPECT_FALSE(device.IsAttn());
    EXPECT_TRUE(device.IsRemoved());
    EXPECT_FALSE(device.IsLocked());
    EXPECT_TRUE(device.IsStopped());
}

TEST(DeviceTest, EnrichStatistics)
{
    NiceMock<MockDevice> device(0);

    vector<PbStatistics> statistics;

    device.EnrichStatistics(statistics, CATEGORY_INFO, "test", 123);
    EXPECT_EQ(1U, statistics.size());
    const PbStatistics &s = statistics[0];
    EXPECT_EQ(CATEGORY_INFO, s.category());
    EXPECT_EQ("test", s.key());
    EXPECT_EQ(123U, s.value());
}
