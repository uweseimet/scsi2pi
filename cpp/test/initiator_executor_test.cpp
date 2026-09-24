//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "initiator/initiator_executor.h"

TEST(InitiatorExecutorTest, Execute)
{
    MockBus bus;
    InitiatorExecutor executor(bus, 0, *default_logger());

    EXPECT_EQ(0xff, executor.Execute( { }, { }, 0, 0, false, false));
}

TEST(InitiatorExecutorTest, SetLimit)
{
    MockBus bus;
    InitiatorExecutor executor(bus, 0, *default_logger());

    EXPECT_TRUE(executor.SetLimit(1));
    EXPECT_TRUE(executor.SetLimit(0));
    EXPECT_FALSE(executor.SetLimit(-1));
}
