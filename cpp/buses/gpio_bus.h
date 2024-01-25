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

#include <spdlog/spdlog.h>
#include "buses/bus.h"

//---------------------------------------------------------------------------
//
//	Connection method definitions
//
//---------------------------------------------------------------------------
//#define BOARD_STANDARD // Standard (SCSI logic, standard pin assignment)
//#define BOARD_FULLSPEC // Full spec (SCSI logic, standard pin assignment)
//#define BOARD_AIBOM // AIBOM version (positive logic, unique pin assignment)
//#define BOARD_GAMERNIUM // GAMERnium.com version (standard logic, unique pin assignment)

#if defined BOARD_STANDARD
#include "buses/connection_type/connection_standard.h"
#elif defined BOARD_FULLSPEC
#include "buses/connection_type/connection_fullspec.h"
#elif defined BOARD_AIBOM
#include "buses/connection_type/connection_aibom.h"
#elif defined BOARD_GAMERNIUM
#include "buses/connection_type/connection_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

using namespace std;

//---------------------------------------------------------------------------
//
// SIGNAL_CONTROL_MODE: Signal control mode selection
//  You can customize the signal control logic from Version 1.22
//
// 0: SCSI logical specification
//     Conversion board using 74LS641-1 etc. directly connected or published on HP
//   True  : 0V
//   False : Open collector output (disconnect from bus)
//
// 1: Negative logic specification (when using conversion board for negative logic -> SCSI logic)
//     There is no conversion board with this specification at this time
//   True  : 0V   -> (CONVERT) -> 0V
//   False : 3.3V -> (CONVERT) -> Open collector output
//
// 2: Positive logic specification (when using the conversion board for positive logic -> SCSI logic)
//     PiSCSI Adapter Rev.C @132sync etc.
//
//   True  : 3.3V -> (CONVERT) -> 0V
//   False : 0V   -> (CONVERT) -> Open collector output
//
//---------------------------------------------------------------------------

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

// Constant declarations (GPIO)
const static int GPIO_INPUT = 0;
const static int GPIO_OUTPUT = 1;
const static int GPIO_IRQ_IN = 3;
const static int GPIO_PULLNONE = 0;
const static int GPIO_PULLDOWN = 1;
const static int GPIO_PULLUP = 2;

// Constant declarations (Control signals)
#define ACT_OFF !ACT_ON
#define ENB_OFF !ENB_ON
#define TAD_OUT !TAD_IN
#define IND_OUT !IND_IN
#define DTD_OUT !DTD_IN

// Constant declarations (SCSI)
#define IN GPIO_INPUT
#define OUT GPIO_OUTPUT
const static int ON = 1;
const static int OFF = 0;

class GpioBus : public Bus
{

public:

    bool Init(bool = true) override;

    int CommandHandShake(vector<uint8_t>&) override;
    int ReceiveHandShake(uint8_t*, int) override;
    int SendHandShake(uint8_t*, int, int = SEND_NO_DELAY) override;

protected:

    bool IsTarget() const
    {
        return target_mode;
    }

    virtual bool WaitSignal(int, bool);

    virtual bool WaitREQ(bool) = 0;
    virtual bool WaitACK(bool) = 0;

    virtual void EnableIRQ() = 0;
    virtual void DisableIRQ() = 0;

    // Set GPIO output signal
    virtual void PinSetSignal(int, bool) = 0;

private:

    bool target_mode = true;

    // The DaynaPort SCSI Link do a short delay in the middle of transfering
    // a packet. This is the number of ns that will be delayed between the
    // header and the actual data.
    inline const static int SCSI_DELAY_SEND_DATA_DAYNAPORT_NS = 100'000;
};
