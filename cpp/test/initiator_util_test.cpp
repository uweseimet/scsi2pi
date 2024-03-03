//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "initiator/initiator_util.h"

using namespace initiator_util;

TEST(InitiatorUtilTest, ResetBus)
{
    NiceMock<MockBus> bus;

    EXPECT_CALL(bus, Reset);
    ResetBus(bus);
}
