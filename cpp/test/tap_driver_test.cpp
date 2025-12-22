//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <span>
#include <gtest/gtest.h>
#include "devices/tap_driver.h"

TEST(TapDriverTest, HasPendingPackets)
{
    TapDriver tap_driver;

    EXPECT_FALSE(tap_driver.HasPendingPackets());
}

TEST(TapDriverTest, Receive)
{
    TapDriver tap_driver;

    EXPECT_EQ(0, tap_driver.Receive( { }, *default_logger()));
}

TEST(TapDriverTest, Crc32)
{
    array<uint8_t, ETH_FRAME_LEN> buf;

    buf.fill(0x00);
    EXPECT_EQ(0xe3d887bbU, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    buf.fill(0xff);
    EXPECT_EQ(0x814765f4U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    buf.fill(0x10);
    EXPECT_EQ(0xb7288Cd3U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    buf.fill(0x7f);
    EXPECT_EQ(0x4b543477U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    buf.fill(0x80);
    EXPECT_EQ(0x29cbd638U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = static_cast<uint8_t>(i);
    }
    EXPECT_EQ(0xe7870705U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

    for (size_t i = buf.size() - 1; i > 0; --i) {
        buf[i] = static_cast<uint8_t>(i);
    }
    EXPECT_EQ(0xe7870705U, TapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));
}
