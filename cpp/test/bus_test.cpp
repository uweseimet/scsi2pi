//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(BusTest, GetPhase)
{
    EXPECT_EQ(phase_t::dataout, Bus::GetPhase(0b000));
    EXPECT_EQ(phase_t::datain, Bus::GetPhase(0b001));
    EXPECT_EQ(phase_t::command, Bus::GetPhase(0b010));
    EXPECT_EQ(phase_t::status, Bus::GetPhase(0b011));
    EXPECT_EQ(phase_t::reserved, Bus::GetPhase(0b100));
    EXPECT_EQ(phase_t::reserved, Bus::GetPhase(0b101));
    EXPECT_EQ(phase_t::msgout, Bus::GetPhase(0b110));
    EXPECT_EQ(phase_t::msgin, Bus::GetPhase(0b111));

    NiceMock<MockBus> bus;

    EXPECT_EQ(phase_t::busfree, bus.GetPhase());

    ON_CALL(bus, GetSEL()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::selection, bus.GetPhase());

    ON_CALL(bus, GetSEL()).WillByDefault(Return(false));
    ON_CALL(bus, GetBSY()).WillByDefault(Return(true));

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    EXPECT_EQ(phase_t::dataout, bus.GetPhase());
    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::reserved, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::command, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::msgout, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(false));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::datain, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(false));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::reserved, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::msgin, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::status, bus.GetPhase());
}

TEST(BusTest, GetPhaseName)
{
    EXPECT_EQ("BUS FREE", Bus::GetPhaseName(phase_t::busfree));
    EXPECT_EQ("ARBITRATION", Bus::GetPhaseName(phase_t::arbitration));
    EXPECT_EQ("SELECTION", Bus::GetPhaseName(phase_t::selection));
    EXPECT_EQ("RESELECTION", Bus::GetPhaseName(phase_t::reselection));
    EXPECT_EQ("COMMAND", Bus::GetPhaseName(phase_t::command));
    EXPECT_EQ("DATA IN", Bus::GetPhaseName(phase_t::datain));
    EXPECT_EQ("DATA OUT", Bus::GetPhaseName(phase_t::dataout));
    EXPECT_EQ("STATUS", Bus::GetPhaseName(phase_t::status));
    EXPECT_EQ("MESSAGE IN", Bus::GetPhaseName(phase_t::msgin));
    EXPECT_EQ("MESSAGE OUT", Bus::GetPhaseName(phase_t::msgout));
    EXPECT_EQ("reserved", Bus::GetPhaseName(phase_t::reserved));
}
