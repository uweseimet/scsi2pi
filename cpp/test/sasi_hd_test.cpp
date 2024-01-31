//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

TEST(SasiHdTest, Inquiry)
{
    auto [controller, hd] = CreateDevice(SAHD);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(hd->Dispatch(scsi_command::cmd_inquiry));
    const vector<uint8_t> &buffer = controller->GetBuffer();
    EXPECT_EQ(0, buffer[0]);
    EXPECT_EQ(0, buffer[1]);
}

TEST(SasiHdTest, RequestSense)
{
    const int LUN = 1;
    auto [controller, hd] = CreateDevice(SAHD, LUN);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    EXPECT_NO_THROW(hd->Dispatch(scsi_command::cmd_request_sense));
    const vector<uint8_t> &buffer = controller->GetBuffer();
    EXPECT_EQ(0, buffer[0]);
    EXPECT_EQ(LUN << 5, buffer[1]);
}

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
    EXPECT_EQ(3U, sector_sizes.size());

    EXPECT_TRUE(sector_sizes.contains(256));
    EXPECT_TRUE(sector_sizes.contains(512));
    EXPECT_TRUE(sector_sizes.contains(1024));
}
