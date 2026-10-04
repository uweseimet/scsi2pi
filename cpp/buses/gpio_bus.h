//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <linux/gpio.h>
#include "bus.h"

class GpioBus : public Bus
{

public:

    bool IsRaspberryPi() const override
    {
        return true;
    }

    void CleanUp() override;

    void Reset() const override;

    void SetBSY(bool) const override;

    void SetSEL(bool) const override;

    void SetDataDirIn(bool) const override;

    uint8_t WaitForSelection() override;

protected:

    explicit GpioBus(bool);

    // Checks the board type and sets the CPU affinity
    string SetUp(bool) override;

    // Registers the SEL signal event and creates the epoll instance used by WaitForSelection()
    string SetUpSelectionEvent();

    // Sets a control pin (not a SCSI signal) to the requested level
    virtual void PinSetSignal(int, bool) const = 0;

    // Set to -1 for the STANDARD board
    int pin_ind = PIN_IND;
    int pin_tad = PIN_TAD;
    int pin_dtd = PIN_DTD;

    static constexpr auto SIGNAL_TABLE = to_array<int>( { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
        PIN_DT7, PIN_DP, PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY, PIN_MSG, PIN_CD, PIN_IO, PIN_REQ });

private:

    // SEL signal event request
    gpioevent_request selevreq = { };

    int epoll_fd = -1;
};
