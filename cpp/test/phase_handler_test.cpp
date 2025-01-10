//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/phase_handler.h"

TEST(PhaseHandlerTest, Phases)
{
    MockPhaseHandler handler;

    handler.SetPhase(BusPhase::SELECTION);
    EXPECT_TRUE(handler.IsSelection());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::BUS_FREE);
    EXPECT_TRUE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::COMMAND);
    EXPECT_TRUE(handler.IsCommand());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::STATUS);
    EXPECT_TRUE(handler.IsStatus());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::DATA_IN);
    EXPECT_TRUE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::DATA_OUT);
    EXPECT_TRUE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::MSG_IN);
    EXPECT_TRUE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(BusPhase::MSG_OUT);
    EXPECT_TRUE(handler.IsMsgOut());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
}

TEST(PhaseHandlerTest, ProcessPhase)
{
    MockPhaseHandler handler;
    handler.Init();

    handler.SetPhase(BusPhase::SELECTION);
    EXPECT_CALL(handler, Selection);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::BUS_FREE);
    EXPECT_CALL(handler, BusFree);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::DATA_IN);
    EXPECT_CALL(handler, DataIn);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::DATA_OUT);
    EXPECT_CALL(handler, DataOut);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::COMMAND);
    EXPECT_CALL(handler, Command);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::STATUS);
    EXPECT_CALL(handler, Status);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::MSG_IN);
    EXPECT_CALL(handler, MsgIn);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::MSG_OUT);
    EXPECT_CALL(handler, MsgOut);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::ARBITRATION);
    EXPECT_FALSE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::RESELECTION);
    EXPECT_FALSE(handler.ProcessPhase());

    handler.SetPhase(BusPhase::RESERVED);
    EXPECT_FALSE(handler.ProcessPhase());
}
