//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "buses/pin_control.h"
#include "shared/scsi.h"

using namespace std;
using namespace scsi_defs;

// SCSI Bus timings taken from https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-05.html
const static int SCSI_DELAY_ARBITRATION_DELAY_NS = 2400;
const static int SCSI_DELAY_ASSERTION_PERIOD_NS = 90;
const static int SCSI_DELAY_BUS_CLEAR_DELAY_NS = 800;
const static int SCSI_DELAY_BUS_FREE_DELAY_NS = 800;
const static int SCSI_DELAY_BUS_SET_DELAY_NS = 1800;
const static int SCSI_DELAY_BUS_SETTLE_DELAY_NS = 400;
const static int SCSI_DELAY_CABLE_SKEW_DELAY_NS = 10;
const static int SCSI_DELAY_DATA_RELEASE_DELAY_NS = 400;
const static int SCSI_DELAY_DESKEW_DELAY_NS = 45;
const static int SCSI_DELAY_DISCONNECTION_DELAY_US = 200;
const static int SCSI_DELAY_HOLD_TIME_NS = 45;
const static int SCSI_DELAY_NEGATION_PERIOD_NS = 90;
const static int SCSI_DELAY_POWER_ON_TO_SELECTION_TIME_S = 10; // (recommended)
const static int SCSI_DELAY_RESET_TO_SELECTION_TIME_US = 250 * 1000; // (recommended)
const static int SCSI_DELAY_RESET_HOLD_TIME_US = 25;
const static int SCSI_DELAY_SELECTION_ABORT_TIME_US = 200;
const static int SCSI_DELAY_SELECTION_TIMEOUT_DELAY_NS = 250 * 1000; // (recommended)

class Bus : public PinControl
{

public:

    // Operation modes
    enum class mode_e
    {
        TARGET = 0,
        INITIATOR = 1
    };

    static int GetCommandByteCount(uint8_t);

    virtual bool Init(mode_e) = 0;
    virtual void Reset() = 0;
    virtual void CleanUp() = 0;

    phase_t GetPhase();
    static phase_t GetPhase(int mci)
    {
        return phases[mci];
    }
    static string GetPhaseName(phase_t);

    virtual uint32_t Acquire() = 0;
    virtual int CommandHandShake(vector<uint8_t>&) = 0;
    virtual int ReceiveHandShake(uint8_t*, int) = 0;
    virtual int SendHandShake(uint8_t*, int, int = SEND_NO_DELAY) = 0;

    virtual bool WaitForSelection() = 0;

    virtual bool GetSignal(int) const = 0;
    virtual void SetSignal(int, bool) = 0;

    // Work-around needed for the DaynaPort emulation
    static const int SEND_NO_DELAY = -1;

private:

    static const array<phase_t, 8> phases;

    static const unordered_map<phase_t, string> phase_names;
};
