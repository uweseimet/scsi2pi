//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "initiator/initiator_executor.h"

TEST(InitiatorExecutorTest, GetLogger)
{
    MockBus bus;
    const auto logger = stdout_color_st("initiator_executor_test");
    InitiatorExecutor executor(bus, 0, *logger);

    EXPECT_EQ("initiator_executor_test", executor.GetLogger().name());
}

TEST(InitiatorExecutorTest, FormatBytes)
{
    MockBus bus;
    InitiatorExecutor executor(bus, 0, *default_logger());

    const vector<uint8_t> &bytes = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    EXPECT_EQ("00000000  01:02:03:04:05                                   '.....'",
        executor.FormatBytes(bytes, static_cast<int>(bytes.size())));
}

TEST(InitiatorExecutorTest, SetLimit)
{
    MockBus bus;
    InitiatorExecutor executor(bus, 0, *default_logger());

    const vector<uint8_t> &bytes = { 0x01, 0x02 };
    executor.SetLimit(1);
    EXPECT_EQ("00000000  01                                               '.'\n... (1 more)",
        executor.FormatBytes(bytes, static_cast<int>(bytes.size())));
}
