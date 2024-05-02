//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/phase_handler.h"

TEST(PhaseHandlerTest, Phases)
{
    MockPhaseHandler handler;

    handler.SetPhase(bus_phase::selection);
    EXPECT_TRUE(handler.IsSelection());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::busfree);
    EXPECT_TRUE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::command);
    EXPECT_TRUE(handler.IsCommand());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::status);
    EXPECT_TRUE(handler.IsStatus());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::datain);
    EXPECT_TRUE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::dataout);
    EXPECT_TRUE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::msgin);
    EXPECT_TRUE(handler.IsMsgIn());
    EXPECT_FALSE(handler.IsBusFree());
    EXPECT_FALSE(handler.IsSelection());
    EXPECT_FALSE(handler.IsCommand());
    EXPECT_FALSE(handler.IsStatus());
    EXPECT_FALSE(handler.IsDataIn());
    EXPECT_FALSE(handler.IsDataOut());
    EXPECT_FALSE(handler.IsMsgOut());

    handler.SetPhase(bus_phase::msgout);
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

    handler.SetPhase(bus_phase::selection);
    EXPECT_CALL(handler, Selection);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::busfree);
    EXPECT_CALL(handler, BusFree);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::datain);
    EXPECT_CALL(handler, DataIn);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::dataout);
    EXPECT_CALL(handler, DataOut);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::command);
    EXPECT_CALL(handler, Command);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::status);
    EXPECT_CALL(handler, Status);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::msgin);
    EXPECT_CALL(handler, MsgIn);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::msgout);
    EXPECT_CALL(handler, MsgOut);
    EXPECT_TRUE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::arbitration);
    EXPECT_FALSE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::reselection);
    EXPECT_FALSE(handler.ProcessPhase());

    handler.SetPhase(bus_phase::reserved);
    EXPECT_FALSE(handler.ProcessPhase());
}
