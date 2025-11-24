//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2025 Uwe Seimet
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

//---------------------------------------------------------------------------
//
// SCSI signal pin assignment setting
//   GPIO pin mapping table for SCSI signals.
//   PIN_DT0～PIN_SEL
//
//---------------------------------------------------------------------------

constexpr static int ON = 1;
constexpr static int OFF = 0;

// Control signal pin assignment (-1 means no control)
constexpr static int PIN_ACT = 4; // ACTIVE
constexpr static int PIN_ENB = 5; // ENABLE
#if defined BOARD_FULLSPEC
constexpr static int PIN_IND = 6; // INITIATOR CTRL DIRECTION
constexpr static int PIN_TAD = 7; // TARGET CTRL DIRECTION
constexpr static int PIN_DTD = 8; // DATA DIRECTION
#elif defined BOARD_STANDARD
constexpr static int PIN_IND = -1;
constexpr static int PIN_TAD = -1;
constexpr static int PIN_DTD = -1;
#else
#error Invalid connection type or none specified
#endif

// Control signal output logic
constexpr static int IND_IN = OFF; // INITIATOR SIGNAL INPUT
constexpr static int TAD_IN = OFF; // TARGET SIGNAL INPUT
constexpr static int DTD_IN = ON; // DATA SIGNAL INPUT

// SCSI signal pin assignment
constexpr static int PIN_DT0 = 10;
constexpr static int PIN_DT1 = 11;
constexpr static int PIN_DT2 = 12;
constexpr static int PIN_DT3 = 13;
constexpr static int PIN_DT4 = 14;
constexpr static int PIN_DT5 = 15;
constexpr static int PIN_DT6 = 16;
constexpr static int PIN_DT7 = 17;

// Data parity
constexpr static int PIN_DP = 18;

constexpr static int PIN_ATN = 19;
constexpr static int PIN_ATN_MASK = 1 << PIN_ATN;
constexpr static int PIN_RST = 20;
constexpr static int PIN_RST_MASK = 1 << PIN_RST;
constexpr static int PIN_ACK = 21;
constexpr static int PIN_ACK_MASK = 1 << PIN_ACK;
constexpr static int PIN_REQ = 22;
constexpr static int PIN_REQ_MASK = 1 << PIN_REQ;
constexpr static int PIN_MSG = 23;
constexpr static int PIN_MSG_MASK = 1 << PIN_MSG;
constexpr static int PIN_CD = 24;
constexpr static int PIN_CD_MASK = 1 << PIN_CD;
constexpr static int PIN_IO = 25;
constexpr static int PIN_IO_MASK = 1 << PIN_IO;
constexpr static int PIN_BSY = 26;
constexpr static int PIN_BSY_MASK = 1 << PIN_BSY;
constexpr static int PIN_SEL = 27;
constexpr static int PIN_SEL_MASK = 1 << PIN_SEL;
