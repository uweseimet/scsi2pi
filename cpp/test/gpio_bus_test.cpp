//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(GpioBusTest, SetUp)
{
    MockGpioBus standard_bus(true);
    MockGpioBus full_bus(false);

    EXPECT_EQ("", standard_bus.SetUp(true));
    EXPECT_NE("", standard_bus.SetUp(false));
    EXPECT_EQ("", full_bus.SetUp(true));
    EXPECT_EQ("", full_bus.SetUp(false));
}

TEST(GpioBus, GetPinTad)
{
    MockGpioBus standard_bus(true);
    MockGpioBus full_bus(false);

    EXPECT_EQ(-1, standard_bus.GetPinTad());
    EXPECT_EQ(PIN_TAD, full_bus.GetPinTad());
}

TEST(GpioBus, GetPinInd)
{
    MockGpioBus standard_bus(true);
    MockGpioBus full_bus(false);

    EXPECT_EQ(-1, standard_bus.GetPinInd());
    EXPECT_EQ(PIN_IND, full_bus.GetPinInd());
}

TEST(GpioBus, GetPinDtd)
{
    MockGpioBus standard_bus(true);
    MockGpioBus full_bus(false);

    EXPECT_EQ(-1, standard_bus.GetPinDtd());
    EXPECT_EQ(PIN_DTD, full_bus.GetPinDtd());
}

TEST(GpioBus, SetBSY)
{
    MockGpioBus bus(false);

    EXPECT_CALL(bus, PinSetSignal(PIN_ACT, true));
    EXPECT_CALL(bus, PinSetSignal(bus.GetPinTad(), true));
    bus.SetBSY(true);
}

TEST(GpioBus, SetSEL)
{
    MockGpioBus bus(false);

    EXPECT_CALL(bus, PinSetSignal(PIN_ACT, true));
    bus.SetSEL(true);
}

TEST(GpioBus, SetDataDirIn)
{
    MockGpioBus bus(false);

    EXPECT_CALL(bus, PinSetSignal(bus.GetPinDtd(), true));
    bus.SetDataDirIn(false);
}
