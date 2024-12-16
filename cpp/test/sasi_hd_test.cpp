//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(SasiHdTest, Inquiry)
{
    auto [controller, hd] = CreateDevice(SAHD);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::inquiry));
    auto &buffer = controller->GetBuffer();
    EXPECT_EQ(0, buffer[0]);
    EXPECT_EQ(0, buffer[1]);
}

TEST(SasiHdTest, RequestSense)
{
    const int LUN = 1;
    auto [controller, hd] = CreateDevice(SAHD, LUN);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 4);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::request_sense));
    auto &buffer = controller->GetBuffer();
    EXPECT_EQ(0, buffer[0]);
    EXPECT_EQ(LUN << 5, buffer[1]);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 0);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(hd, scsi_command::request_sense));
    EXPECT_EQ(0, buffer[0]);
    EXPECT_EQ(LUN << 5, buffer[1]);
}

TEST(SasiHdTest, GetBlockSizes)
{
    MockSasiHd hd(0);

    const auto &sizes = hd.GetSupportedBlockSizes();
    EXPECT_EQ(3U, sizes.size());
    EXPECT_TRUE(sizes.contains(256));
    EXPECT_TRUE(sizes.contains(512));
    EXPECT_TRUE(sizes.contains(1024));
}

TEST(SasiHdTest, Open)
{
    MockSasiHd hd(0);

    EXPECT_THROW(hd.Open(), io_exception)<< "Missing filename";

    const path &filename = CreateTempFile(2048);
    hd.SetFilename(filename.string());
    hd.Open();
    EXPECT_EQ(8U, hd.GetBlockCount());
}
