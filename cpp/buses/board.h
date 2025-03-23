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
constexpr static int ACT_ON = ON; // ACTIVE SIGNAL ON
constexpr static int ENB_ON = ON; // ENABLE SIGNAL ON
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
constexpr static int PIN_RST = 20;
constexpr static int PIN_ACK = 21;
constexpr static int PIN_REQ = 22;
constexpr static int PIN_MSG = 23;
constexpr static int PIN_CD = 24;
constexpr static int PIN_IO = 25;
constexpr static int PIN_BSY = 26;
constexpr static int PIN_SEL = 27;
