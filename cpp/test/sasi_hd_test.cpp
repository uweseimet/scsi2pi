//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

TEST(SasiHdTest, FinalizeSetup)
{
    MockSasiHd hd(0);

    hd.SetSectorSizeInBytes(1024);
    EXPECT_THROW(hd.FinalizeSetup(0), io_exception)<< "Device has 0 blocks";
}

TEST(SasiHdTest, GetSectorSizes)
{
    MockSasiHd hd(0);

    const auto &sector_sizes = hd.GetSupportedSectorSizes();
    EXPECT_EQ(3, sector_sizes.size());

    EXPECT_TRUE(sector_sizes.contains(256));
    EXPECT_TRUE(sector_sizes.contains(512));
    EXPECT_TRUE(sector_sizes.contains(1024));
}
