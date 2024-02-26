//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

//
// PiSCSI/SCSI2Pi standard (SCSI logic, standard pin assignment)
//

// Select signal control mode
constexpr static int SIGNAL_CONTROL_MODE = 0; // SCSI logical specification

// Control signal pin assignment (-1 means no control)
constexpr static int PIN_ACT = 4; // ACTIVE
constexpr static int PIN_ENB = 5; // ENABLE
constexpr static int PIN_IND = -1; // INITIATOR CTRL DIRECTION
constexpr static int PIN_TAD = -1; // TARGET CTRL DIRECTION
constexpr static int PIN_DTD = -1; // DATA DIRECTION

// Control signal output logic
#define ACT_ON ON // ACTIVE SIGNAL ON
#define ENB_ON ON // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN ON // DATA SIGNAL INPUT

// SCSI signal pin assignment
constexpr static int PIN_DT0 = 10; // Data 0
constexpr static int PIN_DT1 = 11; // Data 1
constexpr static int PIN_DT2 = 12; // Data 2
constexpr static int PIN_DT3 = 13; // Data 3
constexpr static int PIN_DT4 = 14; // Data 4
constexpr static int PIN_DT5 = 15; // Data 5
constexpr static int PIN_DT6 = 16; // Data 6
constexpr static int PIN_DT7 = 17; // Data 7
constexpr static int PIN_DP = 18; // Data parity
constexpr static int PIN_ATN = 19; // ATN
constexpr static int PIN_RST = 20; // RST
constexpr static int PIN_ACK = 21; // ACK
constexpr static int PIN_REQ = 22; // REQ
constexpr static int PIN_MSG = 23; // MSG
constexpr static int PIN_CD = 24; // CD
constexpr static int PIN_IO = 25; // IO
constexpr static int PIN_BSY = 26; // BSY
constexpr static int PIN_SEL = 27; // SEL
