//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "test_shared.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"

using namespace protobuf_util;
using namespace testing;

TEST(ProtobufUtil, SerializeMessage)
{
    PbResult result;

    const int fd = open("/dev/null", O_WRONLY);
    ASSERT_NE(-1, fd);
    SerializeMessage(fd, result);
    close(fd);
    EXPECT_THROW(SerializeMessage(-1, result), IoException)<< "Writing a message must fail";
}

TEST(ProtobufUtil, DeserializeMessage)
{
    PbResult result;
    vector<byte> buf(1);

    int fd = open("/dev/null", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_THROW(DeserializeMessage(fd, result), IoException)<< "Reading the message header must fail";
    close(fd);

    auto [fd1, filename1] = OpenTempFile();
    // Data size -1
    buf = { byte { 0xff }, byte { 0xff }, byte { 0xff }, byte { 0xff } };
    EXPECT_EQ((ssize_t )buf.size(), write(fd1, buf.data(), buf.size()));
    close(fd1);
    fd1 = open(filename1.c_str(), O_RDONLY);
    ASSERT_NE(-1, fd1);
    EXPECT_THROW(DeserializeMessage(fd1, result), IoException)<< "Invalid header was not rejected";

    auto [fd2, filename2] = OpenTempFile();
    // Data size 2
    buf = { byte { 0x02 }, byte { 0x00 }, byte { 0x00 }, byte { 0x00 } };
    EXPECT_EQ((ssize_t )buf.size(), write(fd2, buf.data(), buf.size()));
    close(fd2);
    fd2 = open(filename2.c_str(), O_RDONLY);
    EXPECT_NE(-1, fd2);
    EXPECT_THROW(DeserializeMessage(fd2, result), IoException)<< "Invalid data were not rejected";
}

TEST(ProtobufUtil, SerializeDeserializeMessage)
{
    PbResult result;
    result.set_status(true);

    auto [fd, filename] = OpenTempFile();
    ASSERT_NE(-1, fd);
    SerializeMessage(fd, result);
    close(fd);

    result.set_status(false);
    fd = open(filename.c_str(), O_RDONLY);
    ASSERT_NE(-1, fd);
    DeserializeMessage(fd, result);
    close(fd);

    EXPECT_TRUE(result.status());
}

TEST(ProtobufUtil, ReadBytes)
{
    vector<byte> buf1(1);
    vector<byte> buf2;

    int fd = open("/dev/null", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_EQ(0U, ReadBytes(fd, buf1));
    EXPECT_EQ(0U, ReadBytes(fd, buf2));
    close(fd);

    fd = open("/dev/zero", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_EQ(1U, ReadBytes(fd, buf1));
    EXPECT_EQ(0U, ReadBytes(fd, buf2));
    close(fd);
}
