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
// RaSCSI Adapter GAMERnium.com version
//

// Select signal control mode
constexpr static int SIGNAL_CONTROL_MODE = 0; // SCSI logical specification

// Control signal output logic
#define ACT_ON ON // ACTIVE SIGNAL ON
#define ENB_ON ON // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN ON // DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
constexpr static int PIN_ACT = 14; // ACTIVE
constexpr static int PIN_ENB = 6; // ENABLE
constexpr static int PIN_IND = 7; // INITIATOR CTRL DIRECTION
constexpr static int PIN_TAD = 8; // TARGET CTRL DIRECTION
constexpr static int PIN_DTD = 5; // DATA DIRECTION

// SCSI signal pin assignment
constexpr static int PIN_DT0 = 21; // Data 0
constexpr static int PIN_DT1 = 26; // Data 1
constexpr static int PIN_DT2 = 20; // Data 2
constexpr static int PIN_DT3 = 19; // Data 3
constexpr static int PIN_DT4 = 16; // Data 4
constexpr static int PIN_DT5 = 13; // Data 5
constexpr static int PIN_DT6 = 12; // Data 6
constexpr static int PIN_DT7 = 11; // Data 7
constexpr static int PIN_DP = 25; // Data parity
constexpr static int PIN_ATN = 10; // ATN
constexpr static int PIN_RST = 22; // RST
constexpr static int PIN_ACK = 24; // ACK
constexpr static int PIN_REQ = 15; // REQ
constexpr static int PIN_MSG = 17; // MSG
constexpr static int PIN_CD = 18; // CD
constexpr static int PIN_IO = 4; // IO
constexpr static int PIN_BSY = 27; // BSY
constexpr static int PIN_SEL = 23; // SEL
