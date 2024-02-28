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
// RaSCSI Adapter Aibom version
//

// Select signal control mode
constexpr static int SIGNAL_CONTROL_MODE = 2; // SCSI positive logic specification

// Control signal output logic
#define ACT_ON ON // ACTIVE SIGNAL ON
#define ENB_ON ON // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN OFF // DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
constexpr static int PIN_ACT = 4; // ACTIVE
constexpr static int PIN_ENB = 17; // ENABLE
constexpr static int PIN_IND = 27; // INITIATOR CTRL DIRECTION
constexpr static int PIN_TAD = -1; // TARGET CTRL DIRECTION
constexpr static int PIN_DTD = 18; // DATA DIRECTION

// SCSI signal pin assignment
constexpr static int PIN_DT0 = 6; // Data 0
constexpr static int PIN_DT1 = 12; // Data 1
constexpr static int PIN_DT2 = 13; // Data 2
constexpr static int PIN_DT3 = 16; // Data 3
constexpr static int PIN_DT4 = 19; // Data 4
constexpr static int PIN_DT5 = 20; // Data 5
constexpr static int PIN_DT6 = 26; // Data 6
constexpr static int PIN_DT7 = 21; // Data 7
constexpr static int PIN_DP = 5; // Data parity
constexpr static int PIN_ATN = 22; // ATN
constexpr static int PIN_RST = 25; // RST
constexpr static int PIN_ACK = 10; // ACK
constexpr static int PIN_REQ = 7; // REQ
constexpr static int PIN_MSG = 9; // MSG
constexpr static int PIN_CD = 11; // CD
constexpr static int PIN_IO = 23; // IO
constexpr static int PIN_BSY = 24; // BSY
constexpr static int PIN_SEL = 8; // SEL
