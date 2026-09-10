//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

//
// SCSI2Pi/PiSCSI standard (SCSI logic, standard pin assignment)
//

//---------------------------------------------------------------------------
//
// Control signal pin assignment setting
//  GPIO pin mapping table for control signals.
//
//  Control signal:
//   PIN_ACT
//     Signal that indicates the status of processing SCSI command.
//   PIN_ENB
//     Signal that indicates the valid signal from start to finish.
//   PIN_TAD
//     Signal that indicates the input/output direction of the target signal (BSY,IO,CD,MSG,REG).
//   PIN_IND
//     Signal that indicates the input/output direction of the initiator signal (SEL, ATN, RST, ACK).
//   PIN_DTD
//     Signal that indicates the input/output direction of the data lines (DT0...DT7,DP).
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// Control signal output logic
//   0V:FALSE  3.3V:TRUE
//
//   ACT_ON
//     PIN_ACT signal
//   ENB_ON
//     PIN_ENB signal
//   TAD_IN
//     PIN_TAD This is the logic when inputting.
//   IND_IN
//     PIN_ENB This is the logic when inputting.
//    DTD_IN
//     PIN_ENB This is the logic when inputting.
//
//---------------------------------------------------------------------------

// Control signal pin assignment
static constexpr int PIN_ACT = 4; // ACTIVE
static constexpr int PIN_ENB = 5; // ENABLE
// These are set to -1 for the STANDARD board in rpi_bus.cpp
static constexpr int PIN_IND = 6; // INITIATOR CTRL DIRECTION
static constexpr int PIN_TAD = 7; // TARGET CTRL DIRECTION
static constexpr int PIN_DTD = 8; // DATA DIRECTION

// SCSI signal pin assignment
static constexpr int PIN_DT0 = 10;
static constexpr int PIN_DT1 = 11;
static constexpr int PIN_DT2 = 12;
static constexpr int PIN_DT3 = 13;
static constexpr int PIN_DT4 = 14;
static constexpr int PIN_DT5 = 15;
static constexpr int PIN_DT6 = 16;
static constexpr int PIN_DT7 = 17;

// Data parity
static constexpr int PIN_DP = 18;

// Control signals
static constexpr int PIN_ATN = 19;
static constexpr int PIN_ATN_MASK = 1 << PIN_ATN;
static constexpr int PIN_RST = 20;
static constexpr int PIN_RST_MASK = 1 << PIN_RST;
static constexpr int PIN_ACK = 21;
static constexpr int PIN_ACK_MASK = 1 << PIN_ACK;
static constexpr int PIN_REQ = 22;
static constexpr int PIN_REQ_MASK = 1 << PIN_REQ;
static constexpr int PIN_MSG = 23;
static constexpr int PIN_MSG_MASK = 1 << PIN_MSG;
static constexpr int PIN_CD = 24;
static constexpr int PIN_CD_MASK = 1 << PIN_CD;
static constexpr int PIN_IO = 25;
static constexpr int PIN_IO_MASK = 1 << PIN_IO;
static constexpr int PIN_BSY = 26;
static constexpr int PIN_BSY_MASK = 1 << PIN_BSY;
static constexpr int PIN_SEL = 27;
static constexpr int PIN_SEL_MASK = 1 << PIN_SEL;
